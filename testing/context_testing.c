#include <ucontext.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 10
#define STACK_SIZE (16*1024)
#define DEBUG 1
#define TIMESLICE 10000

int counters[NUM_THREADS];
ucontext_t contexts[NUM_THREADS];
int current;

void handle_test (int abc) {
	return;
}

void handle_alarm(int ignoreme) {

	static int previous = 0,  next = 0;
	static int bar = 0;
	int i;
	struct itimerval timeQ;

	previous = next;
	next = (next+1)%NUM_THREADS;
	current = next;

	if (DEBUG)
		printf("Handler: %d -> %d\n", previous, next);

	bar = (bar+1)%10;
	if (!bar) {
		for(i = 0; i< NUM_THREADS; i++)
			printf("%d: %d\n", i , counters[i]);
		printf("\n");
	}

	timeQ.it_value.tv_usec = TIMESLICE;
	timeQ.it_value.tv_sec = 0;
	timeQ.it_interval.tv_usec = 0;
	timeQ.it_interval.tv_sec = 0;
	setitimer(ITIMER_PROF, &timeQ, 0);
	swapcontext(contexts+previous, contexts+next);

	return;
}

void foo(int);
int main() {

	int i;
	struct itimerval timeQ;
	struct sigaction sact;

	current = 0;

	for(i = 0; i< NUM_THREADS; i++) {
		counters[i] = 0;
		getcontext(contexts+i);
		contexts[i].uc_stack.ss_sp = malloc(STACK_SIZE);
		contexts[i].uc_stack.ss_size = STACK_SIZE;
		contexts[i].uc_stack.ss_flags = 0;
		contexts[i].uc_link = 0;
		makecontext(contexts+i, foo, 1, i);
	}

	memset(&sact, 0, sizeof(sact));
	sact.sa_handler = handle_alarm;
	sigaction(SIGPROF, &sact, 0);

	timeQ.it_value.tv_usec = TIMESLICE;
	timeQ.it_value.tv_sec = 0;
	timeQ.it_interval.tv_usec = 0;
	timeQ.it_interval.tv_sec = 0;

	setitimer(ITIMER_PROF, &timeQ, 0);
	setcontext(&(contexts[0]));

	return 0;
}

void foo(int me) {

	while(1)
		counters[me]++;
}
