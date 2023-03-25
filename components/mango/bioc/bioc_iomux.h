#ifndef _bioc_IOMUX_H_
#define _bioc_IOMUX_H_

#include "list.h"
#include "bioc_timer.h"

#define IO_EVENT_IN        (1<<0)
#define IO_EVENT_OUT       (1<<1)
#define IO_EVENT_ERR       (1<<2)

typedef struct io_inst {
    list_head_t   self;
    int           fd;
    int           evt;
    char          nonblock;
    int (*on_recv)(struct io_inst *);
    int (*on_writable)(struct io_inst *);
    int (*on_err)(struct io_inst *);
    void         *priv;
} bioc_io_inst_t;

#define IO_INST_INIT(inst, f, et, nblk, rv, wr, er, p)  \
    do {                                                \
        (inst)->fd = f;                                 \
        (inst)->evt = et;                               \
        (inst)->nonblock = nblk;                        \
        (inst)->on_recv = rv;                           \
        (inst)->on_writable = wr;                       \
        (inst)->on_err = er;                            \
        (inst)->priv = p;                               \
    } while (0)

typedef void *bioc_iomux_hdl_t;

extern bioc_iomux_hdl_t bioc_iomux_init(bioc_tmr_hdl_t);
extern void bioc_iomux_deinit(bioc_iomux_hdl_t);
extern void bioc_iomux_add(bioc_iomux_hdl_t, bioc_io_inst_t *);
extern void bioc_iomux_del(bioc_io_inst_t *);
extern void bioc_iomux_loop(bioc_iomux_hdl_t);

#endif
