#include "ut.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

static int allocated_thr; /* number of allocated threads */
static int created_thr; /* number of threads created */
static volatile int curr_thread; /* current running thread */

/* data structures */
static ut_slot_t thr_table[MAX_TAB_SIZE];
static ucontext_t main_ctx;

/* time counter */

#define PROC_ARG_NUM 1
#define CPU_TIME 10000 /* in msec */

int ut_init(int tab_size) {
	if (tab_size < MIN_TAB_SIZE || tab_size > MAX_TAB_SIZE)
		tab_size = MAX_TAB_SIZE;

	allocated_thr = tab_size;

	return 0;
}

tid_t ut_spawn_thread(void (*func)(int), int arg) {
	if (created_thr == allocated_thr)
		return TAB_FULL;
	else if (created_thr > allocated_thr)
		return SYS_ERR;

	if ((thr_table[created_thr].stack = malloc(sizeof(char) * STACKSIZE))
				== NULL) {
		perror("cannot allocate space for thread stack");
		return SYS_ERR;
	}

	thr_table[created_thr].vtime = 0;
	thr_table[created_thr].func = func;
	thr_table[created_thr].arg = arg;

	if (getcontext(&thr_table[created_thr].uc) == -1) {
		perror("cannot init context of thread");
		return SYS_ERR;
	}

	thr_table[created_thr].uc.uc_stack.ss_sp = thr_table[created_thr].stack;
	thr_table[created_thr].uc.uc_stack.ss_size = STACKSIZE;
	thr_table[created_thr].uc.uc_link = &main_ctx;
	/* makecontext doesn't return any value */
	makecontext(&thr_table[created_thr].uc, 
			(void(*)(void))thr_table[created_thr].func, 
			PROC_ARG_NUM, thr_table[created_thr].arg);

	created_thr++;
	return created_thr;
}

void schedule_next(int signal) {
	int prev_thread;

	switch(signal) {
		case SIGVTALRM:
			thr_table[curr_thread].vtime += CPU_TIME / 1000;
			break;
		case SIGALRM:
			alarm(1);
			prev_thread = curr_thread;
			curr_thread = (curr_thread+1) % created_thr;
			if (swapcontext(&thr_table[prev_thread].uc,
						&thr_table[curr_thread].uc) < 0) {
				perror("cannot switch context");
				exit(1);
			}
			break;
		default:
			perror("unknown signal");
			exit(1);
			break;
	}
}

int ut_start(void) {
	struct sigaction sa;
	struct itimerval itv;

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = CPU_TIME;
	itv.it_value = itv.it_interval;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);
	sa.sa_handler = schedule_next;

	if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
		perror("sigaction SIGVTALRM failed");
		return SYS_ERR;
	}

	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction SIGALRM failed");
		return SYS_ERR;
	}

	if (setitimer(ITIMER_VIRTUAL, &itv, NULL) < 0) {
		perror("setitimer failed");
		return SYS_ERR;
	}

	alarm(1);
	curr_thread = 0; /* make sure we start with the first thread */
	swapcontext(&main_ctx, &thr_table[curr_thread].uc);

	return 0;
}

unsigned long ut_get_vtime(tid_t tid) {
	if (tid < created_thr)
		return thr_table[tid].vtime;
	else return 0;
}
