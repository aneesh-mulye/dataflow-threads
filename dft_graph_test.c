#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

const int num_threads = 6;
const int PROF_BLOCKSIZE = 100;

//#define DEBUG_LOG
#define PROF_LOG
#define TIMESLICE 10000
#define SIGWAKE SIGPROF

void foo(int);
void source(int);
void sink(int);
void intermediate(int);
void doubler(int);
void muxer(int);

int main() {

	//return main_thread_test();
	//return main_rr_test();
	return main_compo_test();
}

int main_compo_test() {

	int i;
	unsigned int *order;

	dft_init(6);

	dft_thread_create(source);
	dft_thread_create(doubler);
	dft_thread_create(intermediate);
	dft_thread_create(intermediate);
	dft_thread_create(muxer);
	dft_thread_create(sink);
	dft_thread_link(0, 1);
	dft_thread_link(1, 2);
	dft_thread_link(1, 3);
	dft_thread_link(2, 4);
	dft_thread_link(3, 4);
	dft_thread_link(4, 5);
	
	dft_execute();

	return 0;
}

int main_rr_test() {

	int i;
	unsigned int *order;

	dft_init(num_threads);

	dft_thread_create(sink);
	for(i=1; i < num_threads-1; i++)
		dft_thread_create(intermediate);
	dft_thread_create(source);
	

	for(i = 0; i < (num_threads - 1); i++)
		dft_thread_link(i+1, i);

	dft_execute();

	return 0;

}
int main_sched_test() {

	int i;
	unsigned int *order;

	dft_init(num_threads);

	for(i=0; i < num_threads; i++)
		dft_thread_create(foo);

	for(i = 0; i < (num_threads - 1); i++)
		dft_thread_link(i+1, i);

	dft_execute();

	return 0;

}

int main_thread_test() {

	int i;
	unsigned int *order;

	dft_init(num_threads);

	dft_thread_create(source);
	for(i=1; i < num_threads-1; i++)
		dft_thread_create(intermediate);
	dft_thread_create(sink);

	for(i = 0; i < (num_threads - 1); i++)
		dft_thread_link(i, i+1);

	dft_execute();

	return 0;

}
int main_graph_test() {

	int i;
	unsigned int *order;

	dft_graph_init_test(16);
	//for (i = 0; i < 100; i++)
		//dft_graph_link(&g, i%15, i%13);
	for(i = 0; i < 15; i++) {
		dft_graph_link_test(i+1, i);
	}
	order = malloc(sizeof(unsigned int)*16);

	dft_graph_pretty_test();

	dft_graph_toposort_test(order);

	for(i=0; i<16; i++)
		printf("%d ", order[i]);

	printf("\n");

	return 0;
}


void foo(int me)
{
	while(1) {
		printf("I am thread %d\n", me);
		sleep(1);
		//dft_yield();
	}
}

void source(int me) {
	char a;
	unsigned int i = 0;
	while(1) {
		a = 'a' + (i%26);
		i++;
		dft_write(0, &a, 1);
#ifdef DEBUG_LOG
		printf("I am the source! I produced %c!\n", a);
#endif
		//sleep(1);
		//dft_yield();
	}
}

void intermediate(int me) {

	char temp;

	while(1) {
		dft_read(0, &temp, 1);
		dft_write(0, &temp, 1);
#ifdef DEBUG_LOG
		printf("I am thread %d, who just processed '%c'\n", me, temp);
#endif
		//sleep(1);
		//dft_yield();
	}
}

void sink(int me) {

	char b;
	unsigned long int inv, previnv;
	unsigned long int consumed = 0;
	double bpi;
	sigset_t newset, oldset;
	while(1) {
		dft_read(0, &b, 1);
		consumed++;
#ifdef DEBUG
		printf("I am the sink! I have consumed '%c'! Fear me!\n", b);
#endif
		/*
		dft_read(0, &b, 1);
		printf("I am the sink! I have consumed '%c'! Fear me!\n", b); */
#ifdef PROF_LOG
		if(!(consumed % PROF_BLOCKSIZE)) {
			sigemptyset(&newset);
			sigaddset(&newset, SIGWAKE);
			sigprocmask(SIG_BLOCK, &newset, &oldset);
			printf("Total Consumed: %lu\n", consumed);
			previnv = inv;
			inv = dft_get_invoks();
			printf("Total Scheduler Invocations: %lu\n", inv);
			bpi = (((double)consumed)/(inv*TIMESLICE))*1000000.0;
			printf("Aggregate Throughput: %g bytes/second\n", bpi);
			printf("Invocations for this block: %lu\n",
				inv - previnv);
			printf("Instantaneous throughput = %g bytes/second\n",
			  (((double)PROF_BLOCKSIZE)/((inv-previnv)*TIMESLICE))*
			  1000000.0);
			printf("\n");
			sigprocmask(SIG_SETMASK, &oldset, NULL);
		}
#endif
		//sleep(1);
		//dft_yield();
	}
}

void doubler(int me) {

	char a;
	while(1) {
		dft_read(0, &a, 1);
		dft_write(0, &a, 1);
		dft_write(1, &a, 1);
		//dft_yield();
	}
}

void muxer(int me) {
	char a;
	while(1) {
		dft_read(0, &a, 1);
		dft_write(0, &a, 1);
		dft_read(1, &a, 1);
		dft_write(0, &a, 1);
		//dft_yield();
	}
}
