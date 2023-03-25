#include "bioc_iomux.h"
#include "stdlib.h"
#include "lwip/sockets.h"
#include "arch_dbg.h"

typedef struct {
    list_head_t        io_inst_list;
    bioc_tmr_hdl_t     tmr_hdl;
} bioc_iomux_cb_t;

/*
 * create one IOMUX control instance
 */
bioc_iomux_hdl_t bioc_iomux_init(bioc_tmr_hdl_t tmr_hdl)
{
    bioc_iomux_cb_t *hdl = (bioc_iomux_cb_t *)malloc(sizeof(bioc_iomux_cb_t));
    if (!hdl) return NULL;

    hdl->tmr_hdl = tmr_hdl;

    INIT_LIST_HEAD(&(hdl->io_inst_list));

    return (bioc_iomux_hdl_t)hdl;
}

/*
 * Destroy IOMUX control instance
 */
void bioc_iomux_deinit(bioc_iomux_hdl_t hdl)
{
    bioc_io_inst_t *inst, *tmp;

    bioc_iomux_cb_t *iomux_cb = (bioc_iomux_cb_t *)hdl;

    list_for_each_entry_safe(inst, tmp, &(iomux_cb->io_inst_list), self, bioc_io_inst_t) {
        list_del(&(inst->self));
    }

    free(hdl);
}

/*
 * Add one IO instance to IOMUX
 */
void bioc_iomux_add(bioc_iomux_hdl_t hdl, bioc_io_inst_t *inst)
{
    // set socket to be non-blocking
    if (inst->nonblock)
        fcntl(inst->fd, F_SETFL, fcntl(inst->fd, F_GETFL, 0) | O_NONBLOCK);

    list_add_tail(&(inst->self), &(((bioc_iomux_cb_t *)hdl)->io_inst_list));
}

/*
 * Delete one IO instance from IOMUX
 */
void bioc_iomux_del(bioc_io_inst_t *inst)
{
    list_del(&(inst->self));
    close(inst->fd);
}


/*
 * IOMUX infinite loop and callback for each instance
 */
void bioc_iomux_loop(bioc_iomux_hdl_t hdl)
{
    /* currently we only support read event */
    fd_set rfds, wfds, efds;
    int i, max_fd = -1, nready = 0, rc;
    bioc_io_inst_t *inst;
    bioc_ms_t tick;
    struct timeval timeout;
    bioc_iomux_cb_t *iomux_cb = (bioc_iomux_cb_t *)hdl;

    for (;;) {
        /* prepare the poll fds and timeout to do select */
        max_fd = -1;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        list_for_each_entry(inst, &(iomux_cb->io_inst_list), self, bioc_io_inst_t) {
            if ((inst->evt & (IO_EVENT_IN | IO_EVENT_OUT | IO_EVENT_ERR)) == 0)
                continue;

            if (inst->fd > max_fd) {
                max_fd = inst->fd;
            }

            if (inst->evt & IO_EVENT_IN) {
                FD_SET(inst->fd, &rfds);
            }

            if (inst->evt & IO_EVENT_OUT) {
                FD_SET(inst->fd, &wfds);
            }

            if (inst->evt & IO_EVENT_ERR) {
                FD_SET(inst->fd, &efds);
            }
        }

        rc = bioc_get_next_timeout(iomux_cb->tmr_hdl, &tick);

        /* LOG_DEBUG("next time: %d, rc: %d\r\n", (int)tick, (int)rc); */

        if (rc >= 0) {
            timeout.tv_sec = tick / 1000;
            timeout.tv_usec = (tick - timeout.tv_sec * 1000) * 1000;
        }

        nready = select(max_fd + 1, &rfds, &wfds, &efds, rc < 0 ? NULL : &timeout);

        bioc_time_update(iomux_cb->tmr_hdl);
        bioc_proc_timer(iomux_cb->tmr_hdl);

        if (nready < 0) {
            continue;
        }

        for (i = 0; i < nready; i++) {
            list_for_each_entry(inst, &(iomux_cb->io_inst_list), self, bioc_io_inst_t) {
                if (FD_ISSET(inst->fd, &rfds) && inst->on_recv) {
                    LOG_DEBUG("select %d readable\r\n", inst->fd);
                    inst->on_recv(inst);
                    FD_CLR(inst->fd, &rfds);
                    break;
                }
                if (FD_ISSET(inst->fd, &wfds) && inst->on_writable) {
                    inst->on_writable(inst);
                    FD_CLR(inst->fd, &wfds);
                    break;
                }
                if (FD_ISSET(inst->fd, &efds) && inst->on_err) {
                    inst->on_err(inst);
                    FD_CLR(inst->fd, &efds);
                    break;
                }
            }
        }
    }
}
