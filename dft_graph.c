#include "dft_graph.h"
#include <stdlib.h>
#include <stdio.h>

#define DFT_DEBUG 1

int dft_graph_init(dft_graph_t * graph, unsigned const int size) {

	int i;

	if (!graph)
		return -1;

	graph->adjmat = malloc(size*sizeof(unsigned int *));
	if (!graph->adjmat)
		return -1;

	for(i = 0; i < size; i++) {
		graph->adjmat[i] = malloc(size*sizeof(unsigned int));
		if(!graph->adjmat[i]) {
			if(DFT_DEBUG)
				printf("DEBUG dft_graph_init : %s",
					"adjacency matrix invalid\n");
			return -1;
		}
	}

	graph->size = size;

	return 0;
}

int dft_graph_link(dft_graph_t * graph, unsigned const int source,
		unsigned const int dest) {
	if(!graph)
		return -1;
	if(!graph->adjmat)
		return -1;
	graph->adjmat[source][dest] = 1;
	return 0;
}

int dft_graph_pretty(dft_graph_t * graph) {

	int i, j;

	if(!graph)
		return -1;

	if (!graph->adjmat)
		return -1;

	for(i=0; i<graph->size; i++) {
		printf(">\t");
		for(j=0; j<graph->size; j++) {
			printf("%u ", graph->adjmat[i][j]);
		}
		printf("\n");
	}

	return 0;

}

int dft_graph_toposort(dft_graph_t * graph, unsigned int * order) {

	unsigned int done,
		     *in_degs;
	int i, j;

	if(!graph)
		return -1;
	if(!graph->adjmat)
		return -1;

	done = 0;
	in_degs = malloc(sizeof(unsigned int)*graph->size);
	if(!in_degs)
		return -1;

	for(i = 0; i < graph->size; i++) {
		in_degs[i] = 0;
		for(j = 0; j< graph->size; j++)
			if(graph->adjmat[j][i])
				in_degs[i]++;
	}

	while(done != graph->size) {

		for(i = 0; i < graph->size; i++)
			if(!in_degs[i])
				break;

		if(i == graph->size) {
			free(in_degs);
			if(DFT_DEBUG)
				printf("DEBUG dft_graph_toposort: %s",
						"cycle detected\n");
			return -1;
		}

		in_degs[i] = ~0;

		for(j = 0; j < graph->size; j++)
			if(graph->adjmat[i][j])
				in_degs[j]--;

		order[done] = i;
		done++;
	}

	return 0;
}
