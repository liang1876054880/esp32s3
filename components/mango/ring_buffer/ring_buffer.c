#include <stdlib.h>
#include <string.h>

#include "ring_buffer.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief    -- rb_init :ring buffer without locking
 * @param[ ] -- rb      :ring buffer
 * @return   --
 */
int rb_init(ring_buffer_t **rb, int buff_size)
{
    ring_buffer_t *lrb;
    if (!rb || !buff_size) {
        return -1;
    }

    lrb = (ring_buffer_t *)calloc(1, sizeof(*lrb));
    if (!lrb) {
        return -1;
    }
    *rb = lrb;

    lrb->rp = 0;
    lrb->wp = 0;
    lrb->size = buff_size;
    lrb->buffer = calloc(1, buff_size);
    if (!lrb->buffer) {
        free(lrb);
        return -1;
    }

    return 0;
}

int rb_remove(ring_buffer_t *rb)
{
    if (!rb)
        return -1;

    if (rb->buffer)
        free(rb->buffer);
    free(rb);

    return 0;
}

void rb_reset(ring_buffer_t *rb)
{
    rb->rp = rb->wp = 0;
}

/**
 * @brief    -- rb_count :available data in ring buffer
 * @param[ ] -- rb
 * @return   -- size of available data
 */
int rb_count(ring_buffer_t *rb)
{
    int cnt;

    cnt = (rb->size + rb->wp - rb->rp) % rb->size;

    return cnt;
}

/**
 * @brief    -- rb_full : is ring buffer enough big
 * @param[i] -- rb      : ring buffer
 * @param[i] -- wp      : write pointer
 * @param[i] -- rp      : read pointer
 * @param[i] -- len     : write length
 * @return   -- 0:not full,  1: full
 */
static int rb_full(ring_buffer_t *rb, int wp, int rp, int len)
{
    if ((rb->size - 1 + rp - wp) % rb->size < len) {
        return 1;
    }

    return 0;
}

/**
 * @brief    -- rb_push_back
 * @param[i] -- rb   : ring buffer
 * @param[i] -- data
 * @param[i] -- len
 * @param[i] -- update: keep the data up to date
 * @return   -- err:-1, ok:0
 */
int rb_push_back(ring_buffer_t *rb, char *data, int len, int update)
{
    int wp = rb->wp;
    int rp = rb->rp;

    if (rb_full(rb, wp, rp, len)) {
        if (update) {
            rb->rp = (rp + len) % rb->size; // read pointer backward
        } else {
            return -1; // disable push
        }
    }

    if (wp >= rp) {
        if (len <= (rb->size - wp)) {
            memcpy(rb->buffer + wp, data, len);
        } else {
            memcpy(rb->buffer + wp, data, rb->size - wp);
            memcpy(rb->buffer, data + rb->size - wp, len - rb->size + wp);
        }
    } else {
        memcpy(rb->buffer + wp, data, len);
    }

    rb->wp = (wp + len) % rb->size;

    return 0;
}

/**
 * @brief    -- rb_pop_front
 * @param[i] -- rb     : ring buffer
 * @param[o] -- data   : get data
 * @param[i] -- len
 * @param[i] -- commit : update read pointer
 * @return   -- length
 */
int rb_pop_front(ring_buffer_t *rb, char *data, int len, char commit)
{
    int wp = rb->wp;
    int rp = rb->rp;

    int avail = rb_count(rb);

    if (avail == 0)
        return 0;

    len = MIN(avail, len);

    if (wp > rp) {
        memcpy(data, rb->buffer + rp, len);
    } else {
        if (len <= (rb->size - rp)) {
            memcpy(data, rb->buffer + rp, len);
        } else {
            memcpy(data, rb->buffer + rp, rb->size - rp);
            memcpy(data + rb->size - rp, rb->buffer, len - rb->size + rp);
        }
    }

    if (commit)
        rb->rp = (rp + len) % rb->size;

    return len;
}

