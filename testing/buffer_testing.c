#include <stdio.h>
#include <stdlib.h>

typedef struct {
	void *buf;
	unsigned int size;
	unsigned int head, tail;
} dft_buffer;

int dft_buffer_init(dft_buffer *, unsigned const int);
int dft_buffer_read(dft_buffer *, void *, unsigned const int);
int dft_buffer_write(dft_buffer *, void *, unsigned const int);
int dft_buffer_pretty(dft_buffer *);

int dft_buffer_init(dft_buffer * b, unsigned const int size) {

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

int dft_buffer_write(dft_buffer * b, void * writebuf, unsigned int count) {

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

int dft_buffer_read(dft_buffer * b, void * readbuf, unsigned int count) {

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

int dft_buffer_pretty(dft_buffer * b) {

	unsigned int i;

	if(!b || !b->buf)
		return -1;

	for(i = b->tail + 1; i != b->head; i = (i+1)%(b->size))
		printf("%c", *((char *)b->buf + i));

	return 0;
}

int main() {

	const unsigned int s = 20;
	char b[6] = "asdfg";
	int i;
	dft_buffer abc;

	dft_buffer_init(&abc, s);

	printf("tail: %d\thead: %d\n", abc.tail, abc.head);
	for(i=0; i<s; i++) {
		printf("write: %d\n", dft_buffer_write(&abc, b, 6));
		dft_buffer_pretty(&abc);
		printf("\ntail: %d\thead: %d\n", abc.tail, abc.head);
		printf("read: %d\n", dft_buffer_read(&abc, b, 6));
		dft_buffer_pretty(&abc);
		printf("\ntail: %d\thead: %d\n", abc.tail, abc.head);
	}

	return 0;

}
