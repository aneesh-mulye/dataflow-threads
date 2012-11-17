#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <sys/time.h>
#include <string.h>

#define DFT_DEBUG 1
#define STACK_SIZE (16*1024)
#define TIMESLICE 10000

typedef struct {
	unsigned int **adjmat;
	unsigned int size;
} dft_graph_t;

static dft_graph_t gg;
static ucontext_t * contexts;
static ucontext_t user_context; /* TODO */
static unsigned int current_thread_index;
static unsigned int * thread_order;

static int dft_graph_init(dft_graph_t * graph, unsigned const int size) {

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
static int dft_graph_link(dft_graph_t * graph, unsigned const int source,
		unsigned const int dest) {
	if(!graph)
		return -1;
	if(!graph->adjmat)
		return -1;
	graph->adjmat[source][dest] = 1;
	return 0;
}

static int dft_graph_pretty(dft_graph_t * graph) {

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

static int dft_graph_toposort(dft_graph_t * graph, unsigned int * order) {

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

int dft_graph_init_test(unsigned const int size) {
	return dft_graph_init(&gg, size);
}

int dft_graph_link_test(unsigned const int source, unsigned const int dest) {
	return dft_graph_link(&gg, source, dest);
}

int dft_graph_pretty_test() {
	return dft_graph_pretty(&gg);
}

int dft_graph_toposort_test(unsigned int * order) {
	return dft_graph_toposort(&gg, order);
}

int dft_init(unsigned const int size) {
	contexts = malloc((sizeof(ucontext_t))* size);
	if (!contexts)
		return -1;

	thread_order = malloc((sizeof(unsigned int)*size));
	if(!thread_order) {
		free(contexts);
		return -1;
	}

	if (-1 == dft_graph_init(&gg, size)) {
		free(contexts);
		free(thread_order);
		return -1;
	}

	return 0;
}

int dft_thread_create(void (*tfunc)(int)) {
	static unsigned int index = 0;

	if (index >= gg.size)
		return -1;

	getcontext(contexts + index);
	if(!(contexts[index].uc_stack.ss_sp = malloc(STACK_SIZE)))
		return -1;
	contexts[index].uc_stack.ss_size = STACK_SIZE;
	contexts[index].uc_stack.ss_flags = 0;
	contexts[index].uc_link = 0;
	makecontext(contexts+index, tfunc, 1, index);

	index++;

	return index -1;
}

int dft_execute() {
	if(-1 == dft_graph_toposort(&gg, thread_order))
		return -1;
	setcontext(contexts+thread_order[0]);
}

void dft_schedule() {
	unsigned int previous;

	previous = current_thread_index;
	current_thread_index = (current_thread_index + 1) % (gg.size);
	swapcontext(contexts + thread_order[previous],
			contexts + thread_order[current_thread_index]);

	return;
}

int dft_yield() {
	dft_schedule();
}

int dft_thread_link(unsigned const int source, unsigned const int dest) {

	if(source >= gg.size || dest >= gg.size || source == dest)
		return -1;

	return dft_graph_link(&gg, source, dest);
}
