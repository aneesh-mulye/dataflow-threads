#include <stdlib.h>
#include <stdio.h>

const int num_threads = 6;

void foo(int);
void source(int);
void sink(int);
void intermediate(int);

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

int main() {

	//return main_thread_test();
	return main_rr_test();
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
		printf("I am the source! I produced %c!\n", a);
		//sleep(1);
		dft_yield();
	}
}

void intermediate(int me) {

	char temp;

	while(1) {
		dft_read(0, &temp, 1);
		dft_write(0, &temp, 1);
		printf("I am thread %d, who just processed '%c'\n", me, temp);
		//sleep(1);
		dft_yield();
	}
}

void sink(int me) {

	char b;
	unsigned int x;
	while(1) {
		dft_read(0, &b, 1);
		printf("I am the sink! I have consumed '%c'! Fear me!\n", b);
		x = dft_get_invoks();
		printf("Sched Invoks = %u\n",x);
		//sleep(1);
		dft_yield();
	}
}
