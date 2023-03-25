#include "bioc_timer.h"
#include "bioc.h"

typedef struct tmr_cb {
    list_head_t         tmr_list;
    volatile bioc_ms_t  bioc_sys_msec;
    volatile bioc_ms_t  bioc_tmr_wait;
} bioc_tmr_cb_t;

/* t1 is earlier than t2 */
#define timer_before(tmr1, tmr2) (time_before(tmr1->expire, tmr2->expire))

/**
 * Update the current time.
 * Because all timer's expiring time is relative to current time, so we must
 * update current time after each time-consuming operations.
 */
void bioc_time_update(bioc_tmr_hdl_t hdl)
{
    ((bioc_tmr_cb_t *)hdl)->bioc_sys_msec = arch_os_ms_now();
    return;
}

bioc_tmr_hdl_t bioc_timer_init()
{
    bioc_tmr_cb_t *thdl = (bioc_tmr_cb_t *)malloc(sizeof(bioc_tmr_cb_t));
    if (!thdl) return NULL;

    INIT_LIST_HEAD(&(thdl->tmr_list));
    bioc_time_update((bioc_tmr_hdl_t)thdl);
    thdl->bioc_tmr_wait = 0xFFFFFFFF;

    return (bioc_tmr_hdl_t)thdl;
}

void bioc_timer_deinit(bioc_tmr_hdl_t hdl)
{
    bioc_tmr_t *tmr, *tmp;

    list_for_each_entry_safe(tmr, tmp, &(((bioc_tmr_cb_t *)hdl)->tmr_list), self, bioc_tmr_t) {
        list_del(&(tmr->self));
    }

    free(hdl);
}

/**
 * Place the timer into timer queue.
 */
void bioc_add_timer(bioc_tmr_hdl_t hdl, bioc_tmr_t *timer)
{
    bioc_tmr_t *tmr;
    bioc_tmr_cb_t *tcb = (bioc_tmr_cb_t *)hdl;

    /* bioc_del_timer(timer);  // del timer before add it */

    timer->expire = ((bioc_tmr_cb_t *)hdl)->bioc_sys_msec + timer->val * MS_PER_TICK;
    INIT_LIST_HEAD(&(timer->self));

    list_for_each_entry(tmr, &(((bioc_tmr_cb_t *)hdl)->tmr_list), self, bioc_tmr_t) {
        if (timer_before(timer, tmr)) {
            break;
        }
    }

    list_add_tail(&(timer->self), &(tmr->self));

    if (time_before(timer->expire, tcb->bioc_tmr_wait) && (timer->expire != tcb->bioc_tmr_wait)) {
        //printf("sync ytmr new%lld wait%lld\r\n", timer->expire, tcb->bioc_tmr_wait);
        tcb->bioc_tmr_wait = timer->expire;
        bioc_task_timer_update();
    }
}

/**
 * Reset timer based on last expiration time
 * Place the timer into timer queue.
 */
void bioc_re_add_timer(bioc_tmr_hdl_t hdl, bioc_tmr_t *timer)
{
    bioc_tmr_t *tmr;
    bioc_tmr_cb_t *tcb = (bioc_tmr_cb_t *)hdl;

    timer->expire += timer->val * MS_PER_TICK;
    INIT_LIST_HEAD(&(timer->self));

    list_for_each_entry(tmr, &(((bioc_tmr_cb_t *)hdl)->tmr_list), self, bioc_tmr_t) {
        if (timer_before(timer, tmr)) {
            break;
        }
    }

    list_add_tail(&(timer->self), &(tmr->self));

    if (time_before(timer->expire, tcb->bioc_tmr_wait) && (timer->expire != tcb->bioc_tmr_wait)) {
        tcb->bioc_tmr_wait = timer->expire;
        bioc_task_timer_update();
    }
}

/*
 * timer is running, return 1, other return 0
 */
int bioc_timer_is_running(bioc_tmr_t *timer)
{
    if (timer->self.next != NULL && timer->self.prev != NULL) {
        return 1;
    }
    return 0;
}

/*
 * return the remain time. unit: ms
 */
bioc_ms_t bioc_timer_expire(bioc_tmr_hdl_t hdl, bioc_tmr_t *timer)
{
    bioc_tmr_cb_t *tcb = (bioc_tmr_cb_t *)hdl;

    if (bioc_timer_is_running(timer)) {
        if (time_before(timer->expire, tcb->bioc_sys_msec)) {
            return 0;
        } else {
            return (timer->expire - tcb->bioc_sys_msec);
        }
    }

    return 0;
}

void bioc_del_timer(bioc_tmr_t *timer)
{
    if (timer->self.next != NULL && timer->self.prev != NULL) {
        list_del(&(timer->self));
    }
}


/**
 * Do callbacks for all the expired timer, restart the timer
 * if it's repeatitive.
 */
void bioc_proc_timer(bioc_tmr_hdl_t hdl)
{
    bioc_tmr_cb_t *tcb = (bioc_tmr_cb_t *)hdl;
    bioc_tmr_t *tmr;

    for (;;) {
        if (list_empty(&(tcb->tmr_list))) {
            break;
        }

        tmr = list_first_entry(&(tcb->tmr_list), bioc_tmr_t, self);

        if (time_before(tmr->expire, tcb->bioc_sys_msec)) {
            bioc_del_timer(tmr);
            if (tmr->repeat)
                bioc_add_timer(hdl, tmr);
            tmr->fn(tmr->timer_id, tmr->data);
        } else {
            break;
        }
    }
}


/**
 * Find out how much time can we sleep before we need to
 * wake up to handle the timer.
 */
int bioc_get_next_timeout(bioc_tmr_hdl_t hdl, bioc_ms_t *tick)
{
    bioc_tmr_cb_t *tcb = (bioc_tmr_cb_t *)hdl;
    bioc_tmr_t *tmr;

    if (list_empty(&(tcb->tmr_list))) {
        return -1;
    }

    tmr = list_first_entry(&(tcb->tmr_list), bioc_tmr_t, self);

    if (time_before(tmr->expire, tcb->bioc_sys_msec)) {
        *tick = 0;
    } else {
        *tick = tmr->expire - tcb->bioc_sys_msec;
    }
    tcb->bioc_tmr_wait = tmr->expire;

    return 0;
}
