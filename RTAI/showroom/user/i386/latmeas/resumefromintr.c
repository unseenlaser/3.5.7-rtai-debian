/*
 * Original copyright:
 *
 * COPYRIGHT (C) 2004 Akamina Technologies Inc <www.akamina.com>
 *
 * This module includes a mainline process that collects data using
 * the VMIC timer driver API. The software uses LXRT together with the
 * LXRT extensions necessary to access the functions that are implemented
 * in the driver.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 *
 * This is an extensive rewrite for carrying out the same test on a generic
 * PC, by Paolo Mantegazza <mantegazza@aero.polimi.it>.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/io.h>

#include <rtai_mbx.h>
#include "lxrtext.h"

#define timer MY_TIMER

static int end;
static void *endt(void *args)
{
	while (getchar() != 'E');
	end = 1;
	return 0;
}

static inline int _tmr_get_count(int tmr)
{
	int count;
	outb(0x00, 0x43);
	count = inb(0x40);
	return count | (inb(0x40) << 8);
}

int main(void)
{
	RT_TASK *rtask;
	MBX *mbx;
	int maxisr = 0, maxsched = 0, ovrun = 0, semcnt;
	struct { int isr, sched; } latency;
	int timer_period, timer_freq, h_timer_freq, thread;
	int count, perns, pervar, maxpervar = 0;
	RTIME tp, t;

	rt_allow_nonroot_hrt();
	thread = rt_thread_create(endt, NULL, 10000);
	iopl(3);

	if (!(rtask = rt_task_init_schmod(nam2num("RTASK"), 0, 0, 0, SCHED_FIFO, ALLOWED_CPUS))) {
		printf("CANNOT CREATE REAL TIME TASK\n");
		return 1;
	}
	if (!(mbx = rt_named_mbx_init("LATMBX", 100*sizeof(latency)))) {
		printf("CANNOT CREATE MAILBOX\n");
		rt_task_delete(rtask);
		return 1;
	}

//	rt_make_hard_real_time();
	tmr_get_setup(timer, &timer_period, &timer_freq);
	h_timer_freq = timer_freq/2;
	perns = timer_period*1000000000LL/timer_freq;
 	tmr_start(timer);
#ifdef USE_EXT_WAIT
 	printf("Wait_on_timer returns timer count and cpu time.\n");
#else
 	printf("Wait_on_timer does just that.\n");
#endif
#ifdef DISABLE_INTR
 	printf("Wait_on_timer and times getting with interrupt disabled.\n");
#else
 	printf("Wait_on_timer and times getting with interrupt enabled.\n");
#endif
 	printf("Timer setup completed, running.\n\n");

	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while (wait_on_timer(timer) > 0);
	tp = rt_get_cpu_time_ns();
	while (!end) {
		CLI();
		CHECK_FLAGS();
#ifdef USE_EXT_WAIT
		if ((semcnt = wait_on_timer_ext(timer, &count, &t)) > 0) {
			rt_printk("OVRN: %d, TOT: %d\n", semcnt, ++ovrun);
		}
#else
		if ((semcnt = wait_on_timer(timer)) > 0) {
			rt_printk("OVRN: %d, TOT: %d\n", semcnt, ++ovrun);
		}
		count = tmr_get_count(timer, &t);
#endif
		CHECK_FLAGS();
		STI();
		if ((latency.sched = ((timer_period - count)*1000000 + h_timer_freq)/timer_freq) > maxsched) {
			maxsched = latency.sched;
		}
		if ((latency.isr = ((timer_period - tmr_get_isr_count(timer))*1000000 + h_timer_freq)/timer_freq) > maxisr) {
			maxisr = latency.isr;
		}
		pervar = abs((int)(t - tp));
		tp = t;
		if (pervar > maxpervar) {
			maxpervar = pervar;
		}
		rt_mbx_send_if(mbx, &latency, sizeof(latency));
//		rt_printk("MXI %d, MXS %d\n", maxisr, maxsched);
	}

 	tmr_stop(timer);
	rt_make_soft_real_time();
	rt_task_delete(rtask);
	rt_mbx_delete(mbx);
	printf("*** MAX LATENCIES: ISR %d (us), SCHED %d (us) ***\n", maxisr, maxsched);
	printf("*** MAX PERIOD VARIATION %d (us) ***\n", (maxpervar - perns + 500)/1000);
	return 0;
}
