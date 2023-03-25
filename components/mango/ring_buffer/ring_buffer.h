#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

typedef struct ring_buffer_s {
    char *buffer;    // data buffer
    int  size;       // maximum number of items in the buffer
    int  rp;         // read  index
    int  wp;         // write index
} ring_buffer_t;

int  rb_init(ring_buffer_t **rb, int buff_size);
void rb_reset(ring_buffer_t *rb);
int  rb_count(ring_buffer_t *rb);
int  rb_push_back(ring_buffer_t *rb, char *data, int len, int update);
int  rb_pop_front(ring_buffer_t *rb, char *data, int len, char commit);

#endif /* RING_BUFFER_H_ */
