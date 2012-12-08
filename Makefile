all:
	gcc dft_graph.c dft_graph_test.c -ggdb

demo:
	gcc dft_graph.c demo.c -ggdb
