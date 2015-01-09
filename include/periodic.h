/*
  periodic - a library for repeating periodic events
  Copyright (C) 2008, 2009 David Shaw, <dshaw@jabberwocky.com>

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA
*/

#ifndef _PERIODIC_H_
#define _PERIODIC_H_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  Call this function to add an event to the queue.  Interval is how
  often (in seconds) you want this function to run.  The func is a
  function with the following prototype:

    void my_function(void *arg)

  "now" is the current time.  "arg" is passed as the last argument to
  periodic_event, and is passed through to the func.

  The flags field can be any number of the PERIODIC_xxx flags above,
  ORed together.  DELAY means to start the event after the interval
  (that is, a 5-second event happens after 5 seconds, rather than
  immediately after adding it and then again after 5 seconds).
  ONESHOT means to trigger this event once and never again.

  This function returns a pointer to a struct periodic_event_t on
  success, or NULL on failure (and sets errno).
 */

#define PERIODIC_DELAY   1
#define PERIODIC_ONESHOT 2

struct periodic_event_t *periodic_add(unsigned int interval,unsigned int flags,
				      void (*func)(void *),void *arg);

/*
  Call this function a remove an event from the queue.  Pass the
  struct periodic_event_t pointer returned by periodic_add().  Returns
  0 for success and -1 for failure (and sets errno).
 */

int periodic_remove(struct periodic_event_t *remove);

/*
  Call this function to start the periodic events.  Concurrency is how
  many periodic events can happen at once.  If multiple events happen
  at the same moment and concurrency is not high enough, the events
  will happen one after another rather than concurrently.  The
  NORETURN flag makes periodic_start() not return - the calling thread
  becomes one of the pool used to run periodic events.
 */

#define PERIODIC_DEBUG    1
#define PERIODIC_NORETURN 2

int periodic_start(unsigned int flags);

/*
  Stops calling periodic events.  Note that this does not remove any
  events from the queue.  PERIODIC_WAIT means to wait for all threads
  to exit.  Otherwise, the threads are signalled to exit and may exit
  after periodic_stop() returns.
 */

#define PERIODIC_WAIT 1

int periodic_stop(unsigned int flags);

/*
  A common problem with programs that use any sort of repeating event
  is what to do in the case of a time warp (i.e. a clock that jumps in
  either direction).  For example, if it is 15:00:00 and you have an
  event scheduled for 15:00:08, and the clock jumps back 5 hours to
  10:00:00, then the event that should have happened in 8 seconds
  won't happen for 5 hours and 8 seconds.  This function tells the
  periodic library to detect and handle such and event (by fixing the
  schedule so that events happen on time).  "interval" is how often to
  check for a timewarp, and "warptime" is how much of a jump is
  allowed before the event schedule is fixed.

  You may also pass a pointer to timewarp handler function that will
  be called so your process can do its own timewarp handling in
  addition to the above.  You may pass NULL for this if you do not
  need your own handler function.  The prototype for this handler is
  the same as a periodic event.

  Note that periodic will use a monotonic clock on those platforms
  that support it.  This makes it immune from timewarps for its own
  events (i.e. things added via periodic_add), but you may still need
  this function for other purposes if your program uses wall-clock
  time.
*/

int periodic_timewarp(unsigned int interval,unsigned int warptime,
		      void (*func)(void *),void *arg);

#ifdef __cplusplus
}
#endif

#endif /* !_PERIODIC_H_ */
