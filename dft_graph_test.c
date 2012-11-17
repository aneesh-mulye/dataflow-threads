#include <stdlib.h>
#include <stdio.h>

const int num_threads = 16;

void foo(int);
int main() {

	int i;
	unsigned int *order;

	/* MARK: The following chunk of code is used to test the base graph
	 * functions. None of the *_test functions should be interspersed with
	 * the scheduler functions, as they use the same global graph (gg).
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
	*/

	dft_init(num_threads);

	for(i=0; i < num_threads; i++)
		dft_thread_create(foo);

	for(i = 0; i < (num_threads - 1); i++)
		dft_thread_link(i+1, i);

	dft_execute();

	return 0;

}

void foo(int me)
{
	while(1) {
		printf("I am thread %d\n", me);
		sleep(1);
		dft_yield();
	}
}
