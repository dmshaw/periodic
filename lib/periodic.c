static const char RCSID[]="$Id$";

#include <config.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <periodic.h>

static struct periodic_t
{
  unsigned int interval;
  time_t next_occurance;
  time_t base_time;
  void (*callback)(time_t,void *);
  void *arg;
  struct periodic_t *next;
} *events=NULL;

static pthread_mutex_t event_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event_cond=PTHREAD_COND_INITIALIZER;
static unsigned int concurrency;
static pthread_t *thread;
static pthread_t timewarp;
static unsigned int timewarp_interval;
static unsigned int timewarp_warptime;
static void (*timewarp_callback)(void *);
static void *timewarp_arg;

static void
enqueue(struct periodic_t *event)
{
  /* Reschedule it */

  pthread_mutex_lock(&event_lock);

  printf("adding event %p\n",event);

  event->next_occurance=time(NULL)+event->interval;

  if(event==events)
    abort();

  event->next=events;
  events=event;

  pthread_mutex_unlock(&event_lock);
}

static struct periodic_t *
dequeue(void)
{  
  struct periodic_t *last_event,*next_event;
  int err;

  pthread_mutex_lock(&event_lock);

  for(;;)
    {
      struct periodic_t *event,*last=NULL;
      time_t next_occurance=0x7FFFFFFF; /* 2038 */

      next_event=NULL;

      /* Find the next event to occur */
      for(event=events;event;last=event,event=event->next)
	if(event->next_occurance<next_occurance)
	  {
	    next_occurance=event->next_occurance;
	    next_event=event;
	    last_event=last;
	  }

      /* Now wait for the event time to arrive. */

      if(next_event)
	{
	  struct timespec timeout;

	  printf("waiting until %d\n",(int)next_occurance);

	  timeout.tv_sec=next_occurance;
	  timeout.tv_nsec=0;

	  pthread_cleanup_push((void *)pthread_mutex_unlock,&event_lock);

	  err=pthread_cond_timedwait(&event_cond,&event_lock,&timeout);

	  pthread_cleanup_pop(0);

	  if(err==0)
	    {
	      printf("woken up!\n");
	      /* A new event showed up, so recalculate. */
	      continue;
	    }
	  else
	    printf("timedout\n");
	}
      else
	{
	  /* Wait forever */

	  pthread_cleanup_push((void *)pthread_mutex_unlock,&event_lock);

	  err=pthread_cond_wait(&event_cond,&event_lock);

	  pthread_cleanup_pop(0);

	  if(err==0)
	    {
	      printf("woken up!\n");
	      /* A new event showed up, so recalculate. */
	      continue;
	    }
	  else
	    abort();
	}

      /* If we get to here, we have an event, and its time has been
	 reached. */

      break;
    }

  if(next_event)
    {
      if(last_event)
	{
	  printf("last event\n");
	  last_event->next=next_event->next;
	}
      else
	{
	  printf("next event\n");
	  events=next_event->next;
	}

      next_event->next=NULL;

      printf("removing event %p\n",next_event);
    }

  pthread_mutex_unlock(&event_lock);
  
  return next_event;
}

static void *
periodic_thread(void *foo)
{
  for(;;)
    {
      struct periodic_t *event;
      int err;

      event=dequeue();

      /* Execute it */
      
      (*event->callback)(time(NULL),event->arg);

      enqueue(event);
    }

  /* Never reached */
  return NULL;
}

static void *
timewarp_thread(void *foo)
{
  time_t last_time=time(NULL);

  for(;;)
    {
      time_t now;
      unsigned int remaining=timewarp_interval;

      printf("sleeping\n");

      while(remaining)
	remaining=sleep(remaining);

      now=time(NULL);

      /* last_time+timewarp->interval is where we should be, if there
	 was no timewarp. */

      if(now>last_time+timewarp_interval+timewarp_warptime
	 || now<last_time+timewarp_interval-timewarp_warptime)
	{
	  struct periodic_t *event;

	  /* We've jumped more than warptime seconds, so wake everyone
	     up and make them recalibrate. */

	  printf("recalibrate\n");
	  pthread_mutex_lock(&event_lock);

	  /* Find every event that has a base time that doesn't match,
	     and recalculate its next_occurance */

	  for(event=events;event;event=event->next)
	    event->next_occurance=now+event->interval;

	  pthread_cond_broadcast(&event_cond);
	  pthread_mutex_unlock(&event_lock);

	  if(timewarp_callback)
	    (timewarp_callback)(timewarp_arg);
	}

      last_time=now;
    }

  /* Never reached */
  return NULL;
}

struct periodic_t *
periodic_add(unsigned int interval,unsigned int flags,
	     void (*callback)(time_t,void *),void *arg)
{
  struct periodic_t *event;

  event=calloc(1,sizeof(*event));
  if(!event)
    {
      errno=ENOMEM;
      return NULL;
    }

  event->interval=interval;
  event->callback=callback;
  event->arg=arg;
  if(flags&PERIODIC_DELAY)
    event->next_occurance=time(NULL)+interval;

  pthread_mutex_lock(&event_lock);
  event->next=events;
  events=event;
  pthread_cond_broadcast(&event_cond);
  pthread_mutex_unlock(&event_lock);

  return event;
}

int
periodic_start(unsigned int threads,unsigned int flags)
{
  if(threads==0)
    {
      thread=malloc(sizeof(pthread_t));
      if(!thread)
	{
	  errno=ENOMEM;
	  return -1;
	}

      concurrency=1;
      thread[0]=pthread_self();

      periodic_thread(NULL);
    }
  else
    {
      unsigned int i;

      thread=malloc(sizeof(pthread_t)*threads);
      if(!thread)
	{
	  errno=ENOMEM;
	  return -1;
	}

      for(i=0;i<threads;i++)
	{
	  int err;

	  err=pthread_create(&thread[i],NULL,periodic_thread,NULL);
	  if(err!=0)
	    abort();
	  concurrency++;
	}
    }
}

int
periodic_stop(void)
{
  unsigned int i;
  int err;
  struct periodic_t *event;

  /* Send a cancel to each thread */

  for(i=0;i<concurrency;i++)
    {
      int err;

      err=pthread_cancel(thread[i]);
      if(err!=0)
	goto fail;
    }

  if(timewarp_interval)
    {
      err=pthread_cancel(timewarp);
      if(err!=0)
	goto fail;
    }

  for(i=0;i<concurrency;i++)
    {
      err=pthread_join(thread[i],NULL);
      if(err!=0)
	goto fail;
    }

  if(timewarp_interval)
    {
      err=pthread_join(timewarp,NULL);
      if(err!=0)
	goto fail;
    }

  concurrency=0;
  free(thread);
  timewarp_interval=0;

  pthread_mutex_lock(&event_lock);

  while(events)
    {
      struct periodic_t *event=events;

      events=events->next;

      free(event);
    }

  pthread_mutex_unlock(&event_lock);

  return 0;

 fail:
  errno=err;
  return -1;
}

int
periodic_timewarp(unsigned int interval,unsigned int warptime,
		  void (*callback)(void *),void *arg)
{
  struct timewarp *t;

  if(interval)
    {
      int err;

      timewarp_interval=interval;
      timewarp_warptime=warptime;
      timewarp_callback=callback;
      timewarp_arg=arg;

      err=pthread_create(&timewarp,NULL,timewarp_thread,NULL);
      if(err!=0)
	{
	  errno=err;
	  return -1;
	}
    }

  return 0;
}
