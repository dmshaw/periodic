/* $Id$ */

#ifndef _PERIODIC_H_
#define _PERIODIC_H_

#include <time.h>

#define PERIODIC_DELAY   1
#define PERIODIC_ONESHOT 2

/* Call this function to add an event to the queue.  Interval is how
   often (in seconds) you want this function to run.  The func is a
   function with the following prototype:

     void my_function(time_t now,void *arg)

   "now" is filled in by periodic and is the current time.  "arg" is
   passed to periodic_event, and passed through to the func.

   This function returns a pointer to a struct periodic_event_t on
   success, or NULL on failure (and sets errno).
*/

struct periodic_event_t *periodic_add(unsigned int interval,unsigned int flags,
				      void (*func)(time_t,void *),void *arg);
int periodic_remove(struct periodic_event_t *remove);
int periodic_start(unsigned int concurrency,unsigned int flags);
int periodic_timewarp(unsigned int interval,unsigned int warptime,
		      void (*func)(time_t,void *),void *arg);

#endif /* !_PERIODIC_H_ */
