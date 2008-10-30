/* $Id$ */

#ifndef _PERIODIC_H_
#define _PERIODIC_H_

#include <time.h>

struct periodic_t *periodic_add(unsigned int interval,unsigned int flags,
				void (*routine)(time_t,void *),void *arg);
int periodic_start(unsigned int concurrency,unsigned int flags);
int periodic_stop(void);
int periodic_timewarp(unsigned int interval,unsigned int warptime);

#endif /* !_PERIODIC_H_ */
