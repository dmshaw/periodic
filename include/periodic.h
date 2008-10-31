/* $Id$ */

#ifndef _PERIODIC_H_
#define _PERIODIC_H_

#include <time.h>

#define PERIODIC_DELAY 1

struct periodic_event_t *periodic_add(unsigned int interval,unsigned int flags,
				      void (*callback)(time_t,void *),
				      void *arg);
int periodic_start(unsigned int concurrency,unsigned int flags);
int periodic_stop(void);
int periodic_timewarp(unsigned int interval,unsigned int warptime,
		      void (*callback)(time_t,void *),void *arg);

#endif /* !_PERIODIC_H_ */
