/*
COPYRIGHT (C) 2000  Paolo Mantegazza <mantegazza@aero.polimi.it>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include <rtai_tasklets.h>

#define LOOPS 100

#define TICK_PERIOD 10000000

static struct rt_tasklet_struct *timer;

static int count, data = LOOPS;

static volatile int end;

static void fun(unsigned long data)
{
	if (count++ < LOOPS) {
		rt_printk("FIRED PERIODIC %d %lu\n", count, (*((unsigned int*)data))--);
	} else {
		rt_remove_timer(timer);
		end = 1;
	}
}

int main(void)
{
	struct sched_param mysched;

	mysched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if( sched_setscheduler( 0, SCHED_FIFO, &mysched ) == -1 ) {
		puts(" ERROR IN SETTING THE SCHEDULER UP");
		perror("errno");
		exit(0);
 	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	start_rt_timer(0);
	timer = rt_init_timer();
	rt_insert_timer(timer, 0, rt_get_time() + nano2count(10000000), nano2count(TICK_PERIOD), fun, (unsigned long)&data, 1);
	while(!end) sleep(1);
	stop_rt_timer();
	rt_delete_timer(timer);
	return 0;
}
