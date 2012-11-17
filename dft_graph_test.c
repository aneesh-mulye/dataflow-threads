#include <stdlib.h>
#include <stdio.h>


int main() {

	int i;
	unsigned int *order;

	dft_graph_init_test(16);
	/*
	for (i = 0; i < 100; i++)
		dft_graph_link(&g, i%15, i%13);
	*/
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
