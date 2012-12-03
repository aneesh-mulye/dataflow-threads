#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <sys/time.h>
#include <string.h>

#define DFT_DEBUG 1
#define STACK_SIZE (16*1024)
#define TIMESLICE 10000
#define BUFFER_SIZE 20
#define SIGWAKE SIGPROF

static enum {
	ROUND_ROBIN,
	TOPOLOGICAL,
	IDIOTFILL,
	BACKPROPFILL
} dft_sched_type = BACKPROPFILL;

typedef struct {
	void *buf;
	unsigned int size;
	unsigned int head, tail;
} dft_buffer;

typedef struct {
	dft_buffer ***adjmat;
	unsigned int size;
} dft_graph_t;

typedef struct {
	unsigned int sch_invoks;
	unsigned int read_invoks;
	unsigned int write_invoks;
	unsigned int final_writes;
} dft_stats_t;


static int dft_buffer_init(dft_buffer *, unsigned const int);
static int dft_buffer_read(dft_buffer *, void *, unsigned const int);
static int dft_buffer_write(dft_buffer *, void *, unsigned const int);
static int dft_buffer_pretty(dft_buffer *);
static float dft_buffer_fillratio(dft_buffer *);

static dft_graph_t gg;
static ucontext_t * contexts;
static ucontext_t user_context; /* TODO */
volatile static unsigned int current_thread;
static dft_stats_t stats;
static int (*dft_sched_global)(int *);

int dft_read(unsigned const int, void *, unsigned const int);
int dft_write(unsigned const int, void *, unsigned const int);
void dft_final_write();
unsigned int dft_get_invoks();

void dft_schedule();
int dft_sched_rr(int *);
int dft_sched_topo(int *);
int dft_sched_idiotfill(int*);
int dft_sched_backpropfill(int *);

static int dft_graph_init(dft_graph_t * graph, unsigned const int size) {

	int i, j;

	if (!graph)
		return -1;

	graph->adjmat = malloc(size*sizeof(dft_buffer **));
	if (!graph->adjmat)
		return -1;

	for(i = 0; i < size; i++) {
		graph->adjmat[i] = malloc(size*sizeof(dft_buffer *));
		if(!graph->adjmat[i]) {
			if(DFT_DEBUG)
				printf("DEBUG dft_graph_init : %s",
					"adjacency matrix invalid\n");
			return -1;
		}
		for(j = 0; j < size; j++)
			graph->adjmat[i][j] = NULL;
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

	graph->adjmat[source][dest] = malloc(sizeof(dft_buffer));

	if(!graph->adjmat[source][dest])
		return -1;

	return dft_buffer_init(graph->adjmat[source][dest], BUFFER_SIZE);
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
			printf("%u ", graph->adjmat[i][j] ? 1 : 0);
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

	memset(&stats, 0, sizeof(stats));

	contexts = malloc((sizeof(ucontext_t))* size);
	if (!contexts)
		return -1;

	if (-1 == dft_graph_init(&gg, size)) {
		free(contexts);
		return -1;
	}

	switch (dft_sched_type) {
		case ROUND_ROBIN:
			dft_sched_global = dft_sched_rr;
			break;
		case TOPOLOGICAL:
			dft_sched_global = dft_sched_topo;
			break;
		case IDIOTFILL:
			dft_sched_global = dft_sched_idiotfill;
			break;
		case BACKPROPFILL:
			dft_sched_global = dft_sched_backpropfill;
			break;
		default:
			printf("WTF?! This is not supposed to happen! FIX!\n");
			break; /* TODO: Remove this break. */
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
	struct sigaction sact;
	struct itimerval timeQ;
	int next;
	dft_sched_global(&next);
	current_thread = next;

	memset(&sact, 0, sizeof(sact));
	sact.sa_handler = dft_schedule;
	sigaction(SIGWAKE, &sact, 0);

	timeQ.it_value.tv_usec = TIMESLICE;
	timeQ.it_value.tv_sec = 0;
	timeQ.it_interval.tv_usec = 0;
	timeQ.it_interval.tv_sec = 0;
	setitimer(ITIMER_PROF, &timeQ, 0);
	setcontext(contexts + next);
}

void dft_schedule() {
	unsigned int previous, next;
	struct itimerval timeQ;

	stats.sch_invoks++;

	previous = current_thread;
	dft_sched_global(&next);
	current_thread = next;
	/*
	previous = current_thread_index;
	current_thread_index = (current_thread_index + 1) % (gg.size); */
	timeQ.it_value.tv_usec = TIMESLICE;
	timeQ.it_value.tv_sec = 0;
	timeQ.it_interval.tv_usec = 0;
	timeQ.it_interval.tv_sec = 0;
	setitimer(ITIMER_PROF, &timeQ, 0);
	swapcontext(contexts + previous,
			contexts + next);

	return;
}

int dft_sched_topo(int * next) {
	static int * thread_order = 0x0;
	static int next_index = 0;

	if(!next)
		return -1;

	if(!thread_order) {
		thread_order = malloc((sizeof(unsigned int)*gg.size));
		if(0x0 == thread_order)
			return -1;
		if(-1 == dft_graph_toposort(&gg, thread_order)) {
			free(thread_order);
			return -1;
		}
	}

	*next = thread_order[next_index];
	//printf("dft_sched_topo: will return %d\n", *next);
	next_index = (next_index + 1) % gg.size;

	return 0;
}

int dft_sched_rr(int * next) {

	static int ind = 0;
	if(!next)
		return -1;

	*next = ind;
	ind = (ind + 1) % gg.size;
	return 0;
}

int dft_sched_idiotfill(int * next) {

	static float *prios = 0x0;
	float temp = 0, currmax = 0;
	int i, j;
	if(!next)
		return -1;

	if(!prios) {
		prios = malloc(gg.size * sizeof(float));
		if(!prios)
			return -1;
	}

	memset(prios, 0, sizeof(float)*gg.size);
	for(i=0; i < gg.size; i++)
		for(j=0; j < gg.size; j++)
			if(gg.adjmat[i][j]) {
				temp=0.5-dft_buffer_fillratio(gg.adjmat[i][j]);
				if (temp < 0)
					prios[j] -= temp;
				else
					prios[i] += temp;
			}

	for(i=0; i<gg.size; i++)
		if(currmax < prios[i]) {
			currmax = prios[i];
			*next = i;
		}

	return 0;
}

int dft_sched_backpropfill(int * next) {

	static float *stprios = 0x0, *idprios = 0x0;
	static int *thread_order = 0x0;
	float temp = 0, currmax = 0, margin = 0.05;
	int i, j;
	if(!next)
		return -1;

	if(!stprios) {
		stprios = malloc(gg.size * sizeof(float));
		if(!stprios)
			return -1;
	}
	if(!idprios) {
		idprios = malloc(gg.size * sizeof(float));
		if(!idprios)
			return -1;
	}
	if(!thread_order) {
		thread_order = malloc((sizeof(unsigned int)*gg.size));
		if(0x0 == thread_order)
			return -1;
		if(-1 == dft_graph_toposort(&gg, thread_order)) {
			free(thread_order);
			return -1;
		}
	}

	memset(stprios, 0, sizeof(float)*gg.size);
	memset(idprios, 0, sizeof(float)*gg.size);
	for(i=0; i < gg.size; i++)
		for(j=0; j < gg.size; j++)
			if(gg.adjmat[thread_order[i]][j]) {
				temp = dft_buffer_fillratio(
					gg.adjmat[thread_order[i]][j]);
				if (temp > (0.5+margin)) {
					idprios[j] += (temp - 0.5)*2;
					idprios[j] += (temp - 0.5)*2 *
						idprios[thread_order[i]];
				}
			}
	for(i=gg.size; i >= 0; i--)
		for(j=0; j < gg.size; j++)
			if(gg.adjmat[j][thread_order[i]]) {
				temp = dft_buffer_fillratio(
					gg.adjmat[j][thread_order[i]]);
				if (temp < (0.5-margin)) {
					stprios[j] += (0.5-temp)*2;
					stprios[j] += (0.5 - temp)*2 *
						stprios[thread_order[i]];
				}
			}

	for(i=0; i<gg.size; i++)
		if(currmax < (idprios[i] + stprios[i])) {
			currmax = idprios[i] + stprios[i];
			*next = i;
		}

	return 0;
}

int dft_yield() {
	struct itimerval timeQ;
	sigset_t newset, oldset;
	/* We do not explicitly call schedule here, but set an interval timer
	 * for NOW.
	 */
	/* Disabling signals. This is to ensure that any signals we get are
	 * `collapsed' into a single one at the very end. */
	sigemptyset(&newset);
	sigaddset(&newset, SIGWAKE);
	sigprocmask(SIG_BLOCK, &newset, &oldset);
	timeQ.it_value.tv_usec = 0;
	timeQ.it_value.tv_sec = 0;
	timeQ.it_interval.tv_usec = 0;
	timeQ.it_interval.tv_sec = 0;
	setitimer(ITIMER_PROF, &timeQ, 0);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	dft_schedule();
	/* ^^^ This is (we hope!) as good as a call to dft_schedule(). The
	 * reason for this bizarre-looking way of doing it is to override any
	 * potential wating timers to execute in the middle of a schedule.
	 */
}

int dft_thread_link(unsigned const int source, unsigned const int dest) {

	if(source >= gg.size || dest >= gg.size || source == dest)
		return -1;

	return dft_graph_link(&gg, source, dest);
}

int dft_read(unsigned const int in, void * readbuf, unsigned const int count) {

	unsigned int i, done = -1;
	sigset_t newset, oldset;
	unsigned int cur_thread = current_thread;
	stats.read_invoks++;
	/* The following is not locked because we estimate it doesn't need to
	 * be. We pray it is so.
	 */
	for (i=0; i < gg.size; i++) {
		//if(gg.adjmat[i][current_thread])
		if(gg.adjmat[i][cur_thread])
			done++;
		if(done == in)
			break;
	}

	if(i == gg.size)
		return -1;

	sigemptyset(&newset);
	sigaddset(&newset, SIGWAKE);
	/* TODO:
	 * 1) Make buffer sizes user-specified.
	 * 2) Provide the illusion of infinite-size buffers.
	 */
	done = -1;
	while(done) {
	/* DISABLE SIGNALS HERE */
		sigprocmask(SIG_BLOCK, &newset, &oldset);
	//	done=dft_buffer_read(gg.adjmat[i][current_thread],
	//			readbuf, count);
		done=dft_buffer_read(gg.adjmat[i][cur_thread],
				readbuf, count);
		sigprocmask(SIG_SETMASK, &oldset, NULL);
	/* ENABLE SIGNALS HERE */
	}
	return done;
}

int dft_write(unsigned const int in, void * writebuf,
		unsigned const int count) {
	unsigned int i, done = -1;
	sigset_t newset, oldset;
	unsigned int cur_thread = current_thread;
	stats.write_invoks++;

	/* DISABLE SIGNALS HERE */
	for (i=0; i < gg.size; i++) {
		//if(gg.adjmat[current_thread][i])
		if(gg.adjmat[cur_thread][i])
			done++;
		if(done == in)
			break;
	}

	if(i == gg.size)
		return -1;

	sigemptyset(&newset);
	sigaddset(&newset, SIGWAKE);
	/* TODO:
	 * 1) Make buffer sizes user-specified.
	 * 2) Provide the illusion of infinite-size buffers.
	 */
	done = -1;
	while(done) {
	/* DISABLE SIGNALS HERE */
		sigprocmask(SIG_BLOCK, &newset, &oldset);
		//done=dft_buffer_write(gg.adjmat[current_thread][i],
		//		writebuf, count);
		done=dft_buffer_write(gg.adjmat[cur_thread][i],
				writebuf, count);
		sigprocmask(SIG_SETMASK, &oldset, NULL);
	/* ENABLE SIGNALS HERE */
	}
	return done;
}

static int dft_buffer_init(dft_buffer * b, unsigned const int size) {

	if (!b)
		return -1;

	b->buf = malloc(size+1);
	if(!(b->buf))
		return -1;

	b->size = size+1;

	b->head = 1;
	b->tail = 0;

	return 0;
}

static int dft_buffer_write(dft_buffer * b, void * writebuf, unsigned int count) {

	unsigned int used;
	unsigned int i;

	if(!b || !b->buf || !writebuf)
		return -1;

	used = (b->size + b->head - b->tail - 1)%(b->size);

	if ((used + count) > (b->size-1))
		return -1;

	for(i = 0; i < count; i++) {
		*((char *)b->buf+b->head) = *((char *)writebuf + i);
		b->head = (b->head+1)%(b->size);
	}

	return 0;
}

static float dft_buffer_fillratio(dft_buffer * b) {

	if(!b)
		return -1;

	return ((float)((b->size + b->head - b->tail - 1)%(b->size)))/
		((float)(b->size));
}

static int dft_buffer_read(dft_buffer * b, void * readbuf, unsigned int count) {

	unsigned int used;
	unsigned int i;

	if(!b || !b->buf || !readbuf)
		return -1;

	used = (b->size + b->head - b->tail - 1)%(b->size);

	if(count > used)
		return -1;

	for(i = 0; i < count; i++) {
		*((char *)readbuf + i) =
			*((char *)b->buf + (b->tail + 1)%(b->size));
		b->tail = (b->tail+1)%(b->size);
	}

	return 0;
}

static int dft_buffer_pretty(dft_buffer * b) {

	unsigned int i;

	if(!b || !b->buf)
		return -1;

	for(i = b->tail + 1; i != b->head; i = (i+1)%(b->size))
		printf("%c", *((char *)b->buf + i));

	return 0;
}

void dft_final_write() {
	stats.final_writes++;
	return;
}

unsigned int dft_get_invoks()
{
	return stats.sch_invoks;
}
