static const char RCSID[]="$Id$";

#include <config.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <periodic.h>

static struct periodic_event_t
{
  unsigned int interval;
  time_t next_occurance;
  time_t base_time;
  void (*func)(void *);
  void *arg;
  struct
  {
    unsigned int oneshot:1;
  } flags;
  struct periodic_event_t *next;  
} *events=NULL;

static pthread_mutex_t event_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event_cond=PTHREAD_COND_INITIALIZER;
static pthread_mutex_t thread_lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned int num_threads;
static pthread_t *threads;
static pthread_t timewarp;
static unsigned int timewarp_interval;
static unsigned int timewarp_warptime;
static void (*timewarp_func)(void *);
static void *timewarp_arg;

static void
enqueue(struct periodic_event_t *event)
{
  pthread_mutex_lock(&event_lock);

  if(event->flags.oneshot)
    free(event);
  else
    {
      /* Reschedule it */
      event->next_occurance=time(NULL)+event->interval;

      event->next=events;
      events=event;
    }

  pthread_mutex_unlock(&event_lock);
}

static void
unlocker(void *foo)
{
  pthread_mutex_unlock(&event_lock);
}

static struct periodic_event_t *
dequeue(int *count)
{  
  struct periodic_event_t *last_event=NULL,*next_event;
  int err;

  pthread_mutex_lock(&event_lock);

  for(;;)
    {
      struct periodic_event_t *event,*last=NULL;
      time_t next_occurance=0x7FFFFFFF; /* 2038 */

      *count=0;

      next_event=NULL;

      /* Find the next event to occur */
      for(event=events;event;last=event,event=event->next)
	if(event->next_occurance==next_occurance)
	  (*count)++;
	else if(event->next_occurance<next_occurance)
	  {
	    *count=1;
	    next_occurance=event->next_occurance;
	    next_event=event;
	    last_event=last;
	  }

      /* Now wait for the event time to arrive. */

      if(next_event)
	{
	  struct timespec timeout;

	  timeout.tv_sec=next_occurance;
	  timeout.tv_nsec=0;

	  pthread_cleanup_push(unlocker,NULL);

	  err=pthread_cond_timedwait(&event_cond,&event_lock,&timeout);

	  pthread_cleanup_pop(0);

	  /* A new event showed up, so recalculate.  Also double check
	     to cover for minor clock jitter. */
	  if(err==0 || time(NULL)<next_occurance)
	    continue;
	}
      else
	{
	  /* Wait forever */

	  pthread_cleanup_push(unlocker,NULL);

	  pthread_cond_wait(&event_cond,&event_lock);

	  pthread_cleanup_pop(0);

	  /* A new event showed up, so recalculate. */
	  continue;
	}

      /* If we get to here, we have an event, and its time has been
	 reached. */

      break;
    }

  if(next_event)
    {
      if(last_event)
	last_event->next=next_event->next;
      else
	events=next_event->next;

      next_event->next=NULL;
    }

  pthread_mutex_unlock(&event_lock);
  
  return next_event;
}

static void *
periodic_thread(void *foo)
{
  for(;;)
    {
      struct periodic_event_t *event;
      int count;

      /* Get it */
      event=dequeue(&count);

      /* Execute it */
      (*event->func)(event->arg);

      /* Give it back */
      enqueue(event);
    }

  /* Never reached */
  return NULL;
}

struct periodic_event_t *
periodic_add(unsigned int interval,unsigned int flags,
	     void (*func)(void *),void *arg)
{
  struct periodic_event_t *event;

  event=calloc(1,sizeof(*event));
  if(!event)
    {
      errno=ENOMEM;
      return NULL;
    }

  event->interval=interval;
  event->func=func;
  event->arg=arg;

  if(flags&PERIODIC_DELAY)
    event->next_occurance=time(NULL)+interval;

  if(flags&PERIODIC_ONESHOT)
    event->flags.oneshot=1;

  pthread_mutex_lock(&event_lock);
  event->next=events;
  events=event;
  pthread_cond_broadcast(&event_cond);
  pthread_mutex_unlock(&event_lock);

  return event;
}

int
periodic_remove(struct periodic_event_t *remove)
{
  struct periodic_event_t *event,*last=NULL;

  pthread_mutex_lock(&event_lock);

  for(event=events;event;last=event,event=event->next)
    if(remove==event)
      {
	if(last)
	  last->next=event->next;
	else
	  events=event->next;

	free(event);

	pthread_cond_broadcast(&event_cond);
	break;
      }

  if(!event)
    {
      /* We didn't find it, which indicates that it might be running
	 right now.  Set the oneshot flag to make this into a oneshot
	 event, which will be deleted when the thread that is running
	 it tries to re-enqueue it. */

      event->flags.oneshot=1;
    }

  pthread_mutex_unlock(&event_lock);

  return 0;
}

static void
prepare(void)
{
  pthread_mutex_lock(&event_lock);
}

static void
unprepare(void)
{
  pthread_mutex_unlock(&event_lock);
}

static void
setup_atfork(void)
{
  pthread_atfork(prepare,unprepare,unprepare);
}

int
periodic_start(unsigned int concurrency,unsigned int flags)
{
  static pthread_once_t once=PTHREAD_ONCE_INIT;
  int err;

  pthread_once(&once,setup_atfork);

  if(concurrency==0)
    {
      errno=EINVAL;
      return -1;
    }

  pthread_mutex_lock(&thread_lock);

  if(num_threads)
    {
      pthread_mutex_unlock(&thread_lock);
      errno=EBUSY;
      return -1;
    }

  threads=malloc(sizeof(pthread_t)*concurrency);
  if(!threads)
    {
      pthread_mutex_unlock(&thread_lock);
      errno=ENOMEM;
      return -1;
    }

  if(flags&PERIODIC_NORETURN)
    {
      num_threads=1;
      threads[0]=pthread_self();
      pthread_detach(threads[0]);
    } 

  for(;num_threads<concurrency;num_threads++)
    {
      err=pthread_create(&threads[num_threads],NULL,periodic_thread,NULL);
      if(err==0)
	pthread_detach(threads[num_threads]);
      else
	break;
    }

  if(num_threads!=concurrency)
    {
      unsigned int i;

      /* We failed somewhere, so clean up. */
      for(i=0;i<num_threads;i++)
	if(pthread_self()!=threads[i])
	  pthread_cancel(threads[i]);

      free(threads);
      num_threads=0;

      errno=err;
      return -1;
    }

  pthread_mutex_unlock(&thread_lock);

  if(flags&PERIODIC_NORETURN)
    periodic_thread(NULL);

  return 0;
}

static void *
timewarp_thread(void *foo)
{
  time_t last_time=time(NULL);

  for(;;)
    {
      time_t now;
      unsigned int remaining=timewarp_interval;

      while(remaining)
	remaining=sleep(remaining);

      now=time(NULL);

      /* last_time+timewarp->interval is where we should be, if there
	 was no timewarp. */

      if(now>last_time+timewarp_interval+timewarp_warptime
	 || now<last_time+timewarp_interval-timewarp_warptime)
	{
	  struct periodic_event_t *event;

	  /* We've jumped more than warptime seconds. */

	  if(timewarp_func)
	    (timewarp_func)(timewarp_arg);

	  /* Wake everyone up and make them recalibrate. */

	  pthread_mutex_lock(&event_lock);

	  /* Recalculate everyone */

	  for(event=events;event;event=event->next)
	    event->next_occurance=now+event->interval;

	  pthread_cond_broadcast(&event_cond);
	  pthread_mutex_unlock(&event_lock);

	  /* This is because the timewarp_func may take a while to
	     execute */

	  now=time(NULL);
	}

      last_time=now;
    }

  /* Never reached */
  return NULL;
}

int
periodic_timewarp(unsigned int interval,unsigned int warptime,
		  void (*func)(void *),void *arg)
{
  if(interval)
    {
      int err;

      timewarp_interval=interval;
      timewarp_warptime=warptime;
      timewarp_func=func;
      timewarp_arg=arg;

      err=pthread_create(&timewarp,NULL,timewarp_thread,NULL);
      if(err==0)
	pthread_detach(timewarp);
      else
	{
	  errno=err;
	  return -1;
	}
    }

  return 0;
}
