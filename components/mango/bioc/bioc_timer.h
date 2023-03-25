#ifndef _BIOC_TIMER_H_
#define _BIOC_TIMER_H_

#include <string.h>
#include "list.h"
#include "arch_os.h"

#define MS_PER_TICK      10

typedef unsigned long long bioc_ms_t;
typedef void *bioc_tmr_hdl_t;

typedef void (*timer_func_t)(int timer_id, void *data);

typedef struct {
    list_head_t    self;
    int            timer_id;
    unsigned       val;           /* how many ticks? */
    bioc_ms_t      expire;
    int            repeat;
    timer_func_t   fn;
    void           *data;
} bioc_tmr_t;

#define bioc_start_tmr(hdl, tmr, func, to) \
    do {                                   \
        (tmr)->val = to;                   \
        (tmr)->timer_id = 0;               \
        (tmr)->data = NULL;                \
        (tmr)->fn = func;                  \
        (tmr)->repeat = 0;                 \
        bioc_add_timer(hdl, (tmr));        \
    } while (0)

#define bioc_restart_tmr(hdl, tmr, to) \
    do {                               \
        (tmr)->val = to;               \
        (tmr)->timer_id = 0;           \
        bioc_re_add_timer(hdl, (tmr)); \
    } while (0)

#define bioc_start_tmr_with_data(hdl, tmr, func, to, d) \
    do {                                                \
        (tmr)->val = to;                                \
        (tmr)->timer_id = 0;                            \
        (tmr)->data = d;                                \
        (tmr)->fn = func;                               \
        (tmr)->repeat = 0;                              \
        bioc_add_timer(hdl, (tmr));                     \
    } while (0)

#define bioc_start_repeat_tmr(hdl, tmr, func, to)   \
    do {                                            \
        (tmr)->val = to;                            \
        (tmr)->timer_id = 0;                        \
        (tmr)->data = NULL;                         \
        (tmr)->fn = func;                           \
        (tmr)->repeat = 1;                          \
        bioc_add_timer(hdl, (tmr));                 \
    } while (0)

#define bioc_start_repeat_tmr_with_data(hdl, tmr, func, to, d)  \
    do {                                                        \
        (tmr)->val = to;                                        \
        (tmr)->timer_id = 0;                                    \
        (tmr)->data = d;                                        \
        (tmr)->fn = func;                                       \
        (tmr)->repeat = 1;                                      \
        bioc_add_timer(hdl, (tmr));                             \
    } while (0)

#define time_before(t1, t2) ((long long )((t1)-(t2)) <= 0)

bioc_tmr_hdl_t bioc_timer_init(void);
void bioc_timer_deinit(bioc_tmr_hdl_t);
void bioc_add_timer(bioc_tmr_hdl_t, bioc_tmr_t *);
void bioc_del_timer(bioc_tmr_t *);
int bioc_timer_is_running(bioc_tmr_t *);
bioc_ms_t bioc_timer_expire(bioc_tmr_hdl_t, bioc_tmr_t *);
void bioc_proc_timer(bioc_tmr_hdl_t);
int bioc_get_next_timeout(bioc_tmr_hdl_t, bioc_ms_t *);
void bioc_time_update(bioc_tmr_hdl_t);
void bioc_re_add_timer(bioc_tmr_hdl_t hdl, bioc_tmr_t *timer);

#endif
