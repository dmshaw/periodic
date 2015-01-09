#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <periodic.h>

static void
one(void *foo)
{
  time_t now=time(NULL);

  printf("ONE SECOND EVENT: It is %.24s and my arg is %p\n",ctime(&now),foo);
}

static void
three(void *foo)
{
  time_t now=time(NULL);

  printf("THREE SECOND EVENT: It is %.24s and my arg is %p\n",ctime(&now),foo);
}

static void
five(void *foo)
{
  time_t now=time(NULL);

  printf("FIVE SECOND EVENT: It is %.24s and my arg is %p\n",ctime(&now),foo);
}

static void
oneshot(void *foo)
{
  time_t now=time(NULL);

  printf("ONESHOT EVENT: It is %.24s and my arg is %p\n",ctime(&now),foo);
}

static void
timewarp(void *foo)
{
  time_t now=time(NULL);

  printf("TIMEWARP: It is %.24s and my arg is %p\n",ctime(&now),foo);
}

int
main(int argc,char *argv[])
{
  struct periodic_event_t *event1;

  (void)argc;
  (void)argv;

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
