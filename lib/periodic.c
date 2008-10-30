static const char RCSID[]="$Id$";

#include <config.h>
#include <pthread.h>
#include <time.h>

static struct events
{
  unsigned int interval;
  time_t next_occurance;
  void (*routine)(time_t,void *);
  void *arg;
  struct events *next;
} *events=NULL;

struct periodic_t *
periodic_add(unsigned int interval,void (*routine)(time_t,void *),void *arg)
{
  return NULL;
}
