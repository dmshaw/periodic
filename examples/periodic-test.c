static const char RCSID[]="$Id$";

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <periodic.h>

static void
one(time_t now,void *foo)
{
  printf("ONE SECOND EVENT: It is %d and my arg is %p\n",(int)now,foo);
}

static void
three(time_t now,void *foo)
{
  printf("THREE SECOND EVENT: It is %d and my arg is %p\n",(int)now,foo);
}

static void
five(time_t now,void *foo)
{
  printf("FIVE SECOND EVENT: It is %d and my arg is %p\n",(int)now,foo);
}

static void
oneshot(time_t now,void *foo)
{
  printf("ONESHOT EVENT: It is %d and my arg is %p\n",(int)now,foo);
}

static void
timewarp(time_t now,void *foo)
{
  printf("TIMEWARP: It is %d and my arg is %p\n",(int)now,foo);
}

int
main(int argc,char *argv[])
{
  struct periodic_event_t *event1;

  periodic_timewarp(1,0,timewarp,NULL);

  event1=periodic_add(1,0,one,(void *)0x1234);
  periodic_add(3,0,three,(void *)0x5678);
  periodic_add(5,0,five,(void *)0x5678);

  periodic_add(5,PERIODIC_DELAY|PERIODIC_ONESHOT,oneshot,(void *)0x5678);

  periodic_start(PERIODIC_DEBUG);

  sleep(5);

  periodic_remove(event1);

  pause();

  /* Never reached */
  return 0;
}
