static const char RCSID[]="$Id$";

#include <config.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <periodic.h>

static struct periodic_t
{
  int busy;
  unsigned int interval;
  time_t next_occurance;
  void (*routine)(time_t,void *);
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

static void
unbusy_event(void *e)
{
  struct periodic_t *event=e;

  event->busy=0;
}

static void *
periodic_thread(void *foo)
{
  pthread_mutex_lock(&event_lock);

  for(;;)
    {
      struct periodic_t *event,*next_event=NULL;
      time_t now,next_occurance=0x7FFFFFFF; /* 2038 */
      int err;

      /* Find the next event to occur */
      for(event=events;event;event=event->next)
	if(!event->busy && event->next_occurance<next_occurance)
	  {
	    next_occurance=event->next_occurance;
	    next_event=event;
	  }

      /* Now wait for the event time to arrive. */

      if(next_event)
	{
	  struct timespec timeout;

	  next_event->busy=1;

	  timeout.tv_sec=next_occurance;
	  timeout.tv_nsec=0;

	  pthread_cleanup_push((void *)pthread_mutex_unlock,&event_lock);

	  err=pthread_cond_timedwait(&event_cond,&event_lock,&timeout);

	  pthread_cleanup_pop(0);

	  if(err==0)
	    {
	      printf("woken up!\n");
	      /* A new event showed up, so recalculate. */
	      unbusy_event(next_event);
	      continue;
	    }
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

      /* Execute */

     now=time(NULL);

     /* Check, as we might have been woken up early. */
     if(next_event->next_occurance<=now)
       {
	 /* We unlock while executing, as that can take any amount of
	    time, and other periodic threads may want to work the
	    event list while it executes. */
	 pthread_mutex_unlock(&event_lock);

	 pthread_cleanup_push(unbusy_event,next_event);
	 (*next_event->routine)(now,next_event->arg);
	 /* Reschedule it */
	 next_event->next_occurance=time(NULL)+next_event->interval;
	 pthread_cleanup_pop(1);

	 pthread_mutex_lock(&event_lock);
       }
     else
       unbusy_event(next_event);
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





	  for(event=events;event;event=event->next)
	    if(event->busy)
	      {


	      }



	  pthread_cond_broadcast(&event_cond);
	  pthread_mutex_unlock(&event_lock);
	}

      last_time=now;
    }

  /* Never reached */
  return NULL;
}

struct periodic_t *
periodic_add(unsigned int interval,unsigned int flags,
	     void (*routine)(time_t,void *),void *arg)
{
  struct periodic_t *event;

  event=calloc(1,sizeof(*event));
  if(!event)
    {
      errno=ENOMEM;
      return NULL;
    }

  event->interval=interval;
  event->next_occurance=time(NULL)+interval;
  event->routine=routine;
  event->arg=arg;

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
periodic_timewarp(unsigned int interval,unsigned int warptime)
{
  struct timewarp *t;

  if(interval)
    {
      int err;

      timewarp_interval=interval;
      timewarp_warptime=warptime;

      err=pthread_create(&timewarp,NULL,timewarp_thread,NULL);
      if(err!=0)
	{
	  errno=err;
	  return -1;
	}
    }

  return 0;
}
