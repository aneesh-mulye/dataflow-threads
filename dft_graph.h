typedef struct {
	unsigned int **adjmat;
	unsigned int size;
} dft_graph_t;

int dft_graph_init(dft_graph_t *, unsigned const int size);

int dft_graph_link(dft_graph_t *, unsigned int const source, unsigned const int dest);

/* Assumes that the destination array contains enough space. */
int dft_graph_toposort(dft_graph_t *, unsigned int *);

int dft_graph_pretty(dft_graph_t *);
