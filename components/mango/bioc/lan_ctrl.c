#include "bioc.h"
#include "lan_ctrl.h"

#define  LINE_SEP    "\r\n"
#define  METHOD_LEN  32
#define  TRACE_EXEC() ets_printf("exec %s %d\n", __FUNCTION__, __LINE__)

static lan_clnt_t *search_clnt_by_id(unsigned id);
static int  ctrl_sock_recv(bioc_io_inst_t *);
static int  server_sock_recv(bioc_io_inst_t *);
static int  lan_clnt_recv(bioc_io_inst_t *);
static void clnt_period_cb(int tid, void *data);
static void total_quota_refill(int tid, void *data);
static int  handle_lan_command(lan_clnt_t *clnt, char *data, unsigned len);
static void lan_ctrl_stop(void);
static void lan_ctrl_start(void);
static void lan_clnt_free(lan_clnt_t *clnt);
static void cmd_error_reply(lan_clnt_t *clnt, unsigned err, unsigned cmd_id);

const char *err_msg[] = {
    "ok",
    "invalid command",
    "method not supported",
    "invalid parameter",
    "out of memory",
    "client quota exceeded",
    "fail",
};

lan_ctrl_cb_t  lan_ctrl_cb = {0};

static unsigned clnt_id = 0;

static inline uint32_t net_inet_aton(const char *cp)
{
    struct in_addr addr;
    inet_aton(cp, &addr);
    return addr.s_addr;
}

static void *lan_ctrl_task(void *args)
{
    LOG_INFO("IOMUX loop born ...\r\n");

    bioc_iomux_loop(lan_ctrl_cb.iomux_hdl);

    return NULL;
}

void lan_ctrl_init()
{
    LOG_INFO("enter lan-ctrl init\r\n");

    int rc, sock;
    int flag = 1;
    struct sockaddr_in listen;
    int addr_len;
    BaseType_t res;

    if (lan_ctrl_cb.init) {
        LOG_ERROR("lan ctrl already inited.\r\n");
        return;
    }

    LOG_INFO("init lan control timer\r\n");
    lan_ctrl_cb.tmr_hdl = bioc_timer_init();
    if (!lan_ctrl_cb.tmr_hdl) {
        LOG_ERROR("timer init failed\r\n");
        return;
    }

    LOG_INFO("init lan control multiplexing\r\n");
    lan_ctrl_cb.iomux_hdl = bioc_iomux_init(lan_ctrl_cb.tmr_hdl);

    if (!lan_ctrl_cb.iomux_hdl) {
        LOG_ERROR("io multiplexing init failed\r\n");
        goto err;
    }

    INIT_LIST_HEAD(&(lan_ctrl_cb.clnt_list));

    /* create control socket receive end */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        rc = net_get_sock_error(sock);
        LOG_ERROR("Failed to create control socket: %d.\r\n", rc);
        goto err1;
    }
    LOG_INFO("control socket read end %d\r\n", sock);

    rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));

    if (0 != rc) {
        LOG_ERROR("failed to set SO_REUSEADDR for socket %d, errno=%d rc=%d" LINE_SEP, sock, errno, rc);
        goto err1;
    }

    listen.sin_family = PF_INET;
    listen.sin_port = htons(CTRL_PORT);
    listen.sin_addr.s_addr = net_inet_aton("127.0.0.1");
    addr_len = sizeof(struct sockaddr_in);

    rc = bind(sock, (struct sockaddr *)&listen, addr_len);
    if (rc < 0) {
        LOG_ERROR("Failed to bind control socket\r\n");
        close(sock);
        goto err1;
    }

    IO_INST_INIT(&(lan_ctrl_cb.ctrl_io),
                 sock,
                 IO_EVENT_IN,
                 1,
                 ctrl_sock_recv,
                 NULL,
                 NULL,
                 &(lan_ctrl_cb.ctrl_io)
                );
    bioc_iomux_add(lan_ctrl_cb.iomux_hdl, &(lan_ctrl_cb.ctrl_io));

    if (arch_os_semaphore_create(&lan_ctrl_cb.ctrl_sock_sem) != ERR_OK) {
        LOG_ERROR("create mutex for control socket fail\r\n");
        goto err2;
    }

    /* create control socket send end */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        rc = net_get_sock_error(sock);
        LOG_ERROR("Failed to create control socket client: %d.\r\n", rc);
        goto err3;
    }
    LOG_INFO("control socket send end created %d\r\n", sock);

    // set socket to be non-blocking
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

    lan_ctrl_cb.ctrl_sock.sock = sock;
    memcpy(&(lan_ctrl_cb.ctrl_sock.addr), &listen, addr_len);
    lan_ctrl_cb.ctrl_sock.addr_len = addr_len;

    bioc_start_repeat_tmr(lan_ctrl_cb.tmr_hdl,
                          &(lan_ctrl_cb.quota_refill_tmr),
                          total_quota_refill,
                          QUOTA_REFILL_INTERVAL / MS_PER_TICK);

    lan_ctrl_cb.total_quota_remain = TOTAL_QUOTA_PER_PERIOD;

    lan_ctrl_start();

    arch_os_thread_create(&lan_ctrl_cb.task, "lan_ctrl_task", lan_ctrl_task,
                          4096, NULL, ARCH_OS_PRIORITY_DEFAULT);

    lan_ctrl_cb.init = 1;

    return;

err3:
    arch_os_semaphore_delete(lan_ctrl_cb.ctrl_sock_sem);
err2:
    bioc_iomux_del(&(lan_ctrl_cb.ctrl_io));
err1:
    bioc_iomux_deinit(lan_ctrl_cb.iomux_hdl);
err:
    bioc_timer_deinit(lan_ctrl_cb.tmr_hdl);

    return ;
}

static void cmd_temp_reply(lan_clnt_t *clnt, temp_data_t *temp)
{
    static int id = 0;
    id++;

    char resp[128];
    int rc;

    rc = snprintf(resp, sizeof(resp),
                  "{\"id\":%d, \"temp\":[%f, %f, %f, %f, %f, %f]}\r\n", id,
                  temp->ch0_temp, temp->ch1_temp, temp->ch2_temp, temp->ch3_temp, temp->ch4_temp, temp->ch5_temp);

    rc = send(clnt->io.fd, resp, rc, 0);
    if (rc < 0) {
        LOG_DEBUG("Failed send to client (%d), rc: %d\r\n", clnt->id, net_get_sock_error(clnt->io.fd));
    }
}


void bioc_try_report_temp_data(temp_data_t *temp)
{
    /* lan_clnt_t* clnt = search_clnt_by_id(1); */
    lan_clnt_t *clnt, *tmp;
    list_for_each_entry_safe(clnt, tmp, &(lan_ctrl_cb.clnt_list), self, lan_clnt_t) {
        if (clnt->type == LAN_CLNT_NORMAL) {
            LOG_INFO("--> report temp: %f\r\n", temp->ch0_temp);
            cmd_temp_reply(clnt, temp);
        }
    }
}

static void lan_ctrl_start()
{
    int rc, sock;
    int flag = 1;
    struct sockaddr_in inlisten;
    int addr_len;

    /* fill did */
    uint64_t did;
    uint8_t *dp = (uint8_t *)(&did);
    char device_id[32];

    static uint8_t mac_addr[6] = {0};//定义Mac地址存储空间
    esp_efuse_mac_get_default(mac_addr);//获取mac地址

    if (lan_ctrl_cb.started) {
        LOG_ERROR("Lan control already started\r\n");
        return;
    }

    /* create server socket, listen on :55443 */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        rc = net_get_sock_error(sock);
        LOG_ERROR("Failed to create server socket: %d.\r\n", rc);
        return;
    }

    LOG_INFO("server socket created %d\r\n", sock);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
    inlisten.sin_family = PF_INET;
    inlisten.sin_port = htons(LAN_SERVER_PORT);
    inlisten.sin_addr.s_addr = INADDR_ANY;
    addr_len = sizeof(struct sockaddr_in);

    rc = bind(sock, (struct sockaddr *)&inlisten, addr_len);
    if (rc < 0) {
        LOG_ERROR("Failed to bind lan server socket\r\n");
        close(sock);
        return;
    }

    rc = listen(sock, 2);
    if (rc < 0) {
        LOG_ERROR("Failed to listen on lan server socket\r\n");
        close(sock);
        return;
    }

    IO_INST_INIT(&(lan_ctrl_cb.server_io),
                 sock,
                 IO_EVENT_IN,
                 1,
                 server_sock_recv,
                 NULL,
                 NULL,
                 &(lan_ctrl_cb.server_io)
                );

    bioc_iomux_add(lan_ctrl_cb.iomux_hdl, &(lan_ctrl_cb.server_io));

    did = 1212123123;
    bzero(device_id, sizeof(device_id));

    /* snprintf(device_id, sizeof(device_id), */
    /*          "0x%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx", */
    /*          dp[7], dp[6], dp[5], dp[4], */
    /*          dp[3], dp[2], dp[1], dp[0]); */

    snprintf(device_id, sizeof(device_id),
             "0x%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
             mac_addr[5]);

    rc = lan_discover_init(device_id, lan_ctrl_cb.tmr_hdl, lan_ctrl_cb.iomux_hdl);
    if (rc < 0) {
        LOG_ERROR("Failed to init lan discover!\r\n");
        goto err;
    }

    lan_ctrl_cb.started = 1;

    return;

err:
    bioc_iomux_del(&(lan_ctrl_cb.server_io));

    return ;
}

/**
 * @brief  release lan ctrl memory and delete lan_ctrl_task thread
 *         note.only used when ota coming
 */
static void lan_ctrl_destroy()
{
    lan_ctrl_stop();
    if (lan_ctrl_cb.init == 1) {//lan_ctrl_task was created
        arch_os_thread_delete(lan_ctrl_cb.task);
    }
}

static void lan_ctrl_stop()
{
    lan_clnt_t *clnt, *tmp;

    if (!lan_ctrl_cb.started) {
        LOG_ERROR("Lan control is not yet started\r\n");
        return;
    }

    /* remove listen socket */
    bioc_iomux_del(&(lan_ctrl_cb.server_io));

    /* remove current client */
    list_for_each_entry_safe(clnt, tmp, &(lan_ctrl_cb.clnt_list), self, lan_clnt_t) {
        if (clnt->type == LAN_CLNT_NORMAL)
            lan_clnt_free(clnt);
    }

    /* stop pnp service */
    lan_discover_deinit();

    lan_ctrl_cb.started = 0;

    return ;
}


int ctrl_cmd_send(ctrl_cmd_t *cmd)
{
    int rc;

    if (!lan_ctrl_cb.init)
        return -1;

    LOG_INFO("send command through internal pipe\r\n");

    rc = sendto(lan_ctrl_cb.ctrl_sock.sock, (char *)cmd, sizeof(ctrl_cmd_t), 0,
                (struct sockaddr *) & (lan_ctrl_cb.ctrl_sock.addr),
                lan_ctrl_cb.ctrl_sock.addr_len);
    if (rc < 0)
        LOG_ERROR("failed to send command through internal pipe error %d\r\n", net_get_sock_error(lan_ctrl_cb.ctrl_sock.sock));


    return rc;
}

static void total_quota_refill(int tid, void *data)
{
    LOG_DEBUG("Time to refill the quota for total quota\r\n");
    lan_ctrl_cb.total_quota_remain = TOTAL_QUOTA_PER_PERIOD;
}

static int lan_clnt_new(int fd, lan_clnt_type_t type, struct sockaddr_in *addr_from, socklen_t addr_from_len)
{
    /* set keepalive option to kick out stall TCP client */
    int optval = true;
    lan_clnt_t *clnt = NULL;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                   &optval, sizeof(optval)) == -1) {
        LOG_ERROR("failed to set KEEPALIVE option\r\n");
        close(fd);
        return -1;
    }

    /* TCP Keep-alive idle/inactivity timeout is 10 seconds */
    optval = 10;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,
                   &optval, sizeof(optval)) == -1) {
        LOG_ERROR("failed to set KEEPALIVE idle timeout option\r\n");
        close(fd);
        return -1;
    }

    /* TCP Keep-alive retry count is 3 */
    optval = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,
                   &optval, sizeof(optval)) == -1) {
        LOG_ERROR("failed to set KEEPALIVE retry count option\r\n");
        close(fd);
        return -1;
    }

    /* TCP Keep-alive retry interval (in case no response for probe
     * packet) is 2 seconds.
     */
    optval = 2;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL,
                   &optval, sizeof(optval)) == -1) {
        LOG_ERROR("failed to set KEEPALIVE retry interval option\r\n");
        close(fd);
        return -1;
    }

    clnt = (lan_clnt_t *)malloc(sizeof(lan_clnt_t));
    if (!clnt) {
        return -1;
    }

    if (addr_from)
        clnt->addr = *addr_from;
    clnt->addr_len = addr_from_len;

    if (++clnt_id == 0) /* 0 is reserved for broadcast message */
        clnt_id++;
    clnt->id = clnt_id;

    clnt->type = type;
    list_add_tail(&(clnt->self), &(lan_ctrl_cb.clnt_list));
    lan_ctrl_cb.nr_clnt++;

    IO_INST_INIT(&(clnt->io),
                 fd,
                 IO_EVENT_IN,
                 0, /* blocking, we use DONTWAIT flag in recv */
                 lan_clnt_recv,
                 NULL,
                 NULL,
                 clnt
                );
    bioc_iomux_add(lan_ctrl_cb.iomux_hdl, &(clnt->io));

    clnt->quota_remain = QUOTA_PER_PERIOD;

    if (type != LAN_CLNT_MUSIC)
        bioc_start_repeat_tmr_with_data(lan_ctrl_cb.tmr_hdl, &(clnt->actmr),
                                        clnt_period_cb,
                                        QUOTA_REFILL_INTERVAL,
                                        clnt);

    memset(clnt->msg_buf, 0, sizeof(clnt->msg_buf));
    clnt->msg_offset = 0;

    LOG_INFO("new client id:%d, fd: %d, total client %d\r\n", clnt->id, clnt->io.fd, lan_ctrl_cb.nr_clnt);

    return 0;
}


static int ctrl_sock_recv(bioc_io_inst_t *inst)
{
    int rc;
    ctrl_cmd_t cmd;
    lan_clnt_t *clnt;

    if (arch_os_semaphore_get(lan_ctrl_cb.ctrl_sock_sem, 100) != ERR_OK) {
        LOG_ERROR("failed to lock control socket for receive\n");
        return -1;
    }

    rc = recvfrom(inst->fd, (char *)&cmd, sizeof(cmd),
                  0, NULL, NULL);

    if (rc <= 0) {
        LOG_ERROR("Failed to receive message from ctrl sock %d, error %d\r\n",
                  rc, net_get_sock_error(inst->fd));
        return rc;
    }

    LOG_INFO("receive control message, type:%d\r\n", cmd.cmd);

    if (cmd.cmd == CTRL_CMD_START) {
        lan_ctrl_start();
    } else if (cmd.cmd == CTRL_CMD_STOP) {
        lan_ctrl_stop();
    } else if (cmd.cmd == CTRL_CMD_DESTROY) {
        lan_ctrl_destroy();
    } else if (cmd.cmd == CTRL_CMD_DATA) {
        if (cmd.u.data.clnt_id == 0) {      /* broadcast message */
            LOG_INFO("ctrl_sock_recv broadcast msg: %s\r\n", cmd.u.data.contents);
            list_for_each_entry(clnt, &(lan_ctrl_cb.clnt_list), self, lan_clnt_t) {
                if (clnt->type == LAN_CLNT_MUSIC)
                    continue;
                rc = send(clnt->io.fd, cmd.u.data.contents, strlen(cmd.u.data.contents), 0);
                if (rc < 0) {
                    LOG_ERROR("Failed to send message to client: %d\r\n", clnt->id);
                }
            }
            return 0;
        } else if (cmd.u.data.clnt_id <= MAX_ALLOW_CLNT) {
            clnt = search_clnt_by_id(cmd.u.data.clnt_id);

            if (clnt == NULL) {
                LOG_INFO("Client (%d) gone\r\n", cmd.u.data.clnt_id);
                return -1;
            }

            rc = send(clnt->io.fd, cmd.u.data.contents, strlen(cmd.u.data.contents), 0);

            if (rc < 0) {
                LOG_ERROR("Failed to send, client:%d, fd:%d, rc:%d\r\n",
                          clnt->id, clnt->io.fd,
                          net_get_sock_error(clnt->io.fd));
                return -1;
            }
        } else if (cmd.u.data.clnt_id <= (MAX_ALLOW_CLNT + MAX_ALLOW_CLNT)) {

        }

    } else {
        LOG_ERROR("Invaid command type %d\r\n", cmd.cmd);
    }

    return 0;
}

static int server_sock_recv(bioc_io_inst_t *inst)
{
    int fd, ret;

    struct sockaddr_in addr_from;
    socklen_t addr_from_len;

    fd = accept(inst->fd, (struct sockaddr *)&addr_from, &addr_from_len);
    if (fd < 0) {
        LOG_ERROR("Failed to accept new connection\r\n");
        return -1;
    }

    if (lan_ctrl_cb.nr_clnt >= MAX_ALLOW_CLNT) {
        LOG_ERROR("max allowed client reached\r\n");
        close(fd);
        return -1;
    }

    LOG_INFO("new connect client IP: %s, PORT: %d\r\n",
             inet_ntoa(addr_from.sin_addr), ntohs(addr_from.sin_port));

    ret = lan_clnt_new(fd, LAN_CLNT_NORMAL, &addr_from, addr_from_len);
    if (ret < 0) {
        LOG_ERROR("no mem for new client\r\n");
        close(fd);
        return -1;
    }

    return 0;
}


static void clnt_period_cb(int tid, void *data)
{
    lan_clnt_t *clnt = data;

    LOG_DEBUG("Time to refill the quota for client %d\r\n", clnt->id);

    clnt->quota_remain = QUOTA_PER_PERIOD;
}


static void lan_clnt_free(lan_clnt_t *clnt)
{
    list_del(&(clnt->self));
    lan_ctrl_cb.nr_clnt --;
    bioc_iomux_del(&(clnt->io));
    if (clnt->type == LAN_CLNT_NORMAL)
        bioc_del_timer(&(clnt->actmr));
    free(clnt);
}

static int lan_clnt_recv(bioc_io_inst_t *inst)
{
    lan_clnt_t *clnt = inst->priv;
    int rc;
    void *p;
    unsigned msg_len;

    if (clnt->msg_offset == sizeof(clnt->msg_buf)) {
        LOG_ERROR("No buffer available, message too long\r\n");
        return -1;
    }

    while (1) {
        rc = recv(inst->fd, clnt->msg_buf + clnt->msg_offset,
                  sizeof(clnt->msg_buf) - clnt->msg_offset, MSG_DONTWAIT);

        if (rc <= 0) {
            if (errno == EWOULDBLOCK) {
                LOG_WARN("no unread bytes left\r\n");
            } else {
                LOG_WARN("clnt %d close the socket or error detected, free it\r\n", clnt->id);
                lan_clnt_free(clnt);
            }

            return 0;
        }

        clnt->msg_offset += rc;
        clnt->msg_buf[clnt->msg_offset] = 0;

        for (;;) {
            /* search for message delimiter CRLF */
            p = strstr(clnt->msg_buf, MESSAGE_DELIM);
            if (p == NULL) {
                if (clnt->msg_offset == sizeof(clnt->msg_buf)) {
                    LOG_ERROR("message too long\r\n");
                    memset(clnt->msg_buf, 0, sizeof(clnt->msg_buf));
                    clnt->msg_offset = 0;
                } else {
                    LOG_INFO("partial message, stop handling \r\n");
                }
                break;
            }

            LOG_INFO("total len:%d, found vaild msg len:%d, end flag len:%d\r\n",
                     clnt->msg_offset, (char *)p - clnt->msg_buf, strlen(MESSAGE_DELIM));

            msg_len = (char *)p - clnt->msg_buf;
            clnt->msg_offset = clnt->msg_offset - msg_len - strlen(MESSAGE_DELIM);
            clnt->msg_buf[msg_len] = 0;

            /* LOG_DEBUG("found command len %d, msg_offset change to %d\r\n", msg_len, clnt->msg_offset); */

            rc = handle_lan_command(clnt, clnt->msg_buf, msg_len);
            if (rc < 0) {
                LOG_DEBUG("Failed to handle command\r\n");
            }
            if (clnt->msg_offset) {
                memcpy(clnt->msg_buf, p + strlen(MESSAGE_DELIM), clnt->msg_offset);
            }
            clnt->msg_buf[clnt->msg_offset] = 0;
        }
    }

    return 0;
}


static int handle_lan_command(lan_clnt_t *clnt, char *data, unsigned len)
{
    char method[METHOD_LEN] = "";
    uint32_t id = 0;
    int rc;

    // LOG_DEBUG("handle command len: %d\r\n", len);

    // 数据解析
    /* rc = parse_command(data, len, &id, method); */
    /* if (rc < 0) { */
    /*     LOG_ERROR("Invalid command format, <%s>\r\n", data); */
    /*     cmd_error_reply(clnt, ERR_INVALID_CMD, id); */
    /*     return 0; */
    /* } */

    /* only normal client needs to check quota */
    /* if (clnt->type == LAN_CLNT_NORMAL) { */
    /*     if (clnt->quota_remain == 0 || lan_ctrl_cb.total_quota_remain == 0) { */
    /*         LOG_DEBUG("Client quota exceed, drop the request.\r\n"); */
    /*         cmd_error_reply(clnt, ERR_QUTOA_EXCEED, id); */
    /*         return 0; */
    /*     } */
    /*  */
    /*     [> consume one quota <] */
    /*     clnt->quota_remain --; */
    /*     LOG_DEBUG("Client quota remain: %d\r\n", clnt->quota_remain); */
    /*     lan_ctrl_cb.total_quota_remain --; */
    /*     LOG_DEBUG("Total quota remain: %d\r\n", lan_ctrl_cb.total_quota_remain); */
    /* } */

    LOG_DEBUG("call lan method %d %s %s\n", id, method, data);

    // TODO test
    bioc_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type  = PERIPHERAL_TYPE_NET;

    bzero(peripheral_req.data, sizeof(peripheral_req.data));
    memcpy(peripheral_req.data, data + len - 19, 18);

    BIOC_POST_REQUEST(BIOC_PERIPHERAL_EVT,
                      &peripheral_req, sizeof(bioc_peripheral_req_t), NULL, NULL);

    cmd_error_reply(clnt, ERR_QUTOA_EXCEED, id);

    /* TODO */
    /* rc = call_method(clnt, id, method, data, 0); */
    /* if (rc != 0) { */
    /*     LOG_ERROR("Failed to execute method\r\n"); */
    /*     cmd_error_reply(clnt, rc, id); */
    /* } */

    return 0;
}

static void cmd_error_reply(lan_clnt_t *clnt, unsigned err, unsigned cmd_id)
{
    char resp[128];
    int rc;

    rc = snprintf(resp, sizeof(resp), "{\"id\":%d, \"error\":{\"code\":-1, \"message\":\"%s\"}}\r\n",
                  cmd_id, err_msg[err]);

    rc = send(clnt->io.fd, resp, rc, 0);
    if (rc < 0) {
        LOG_DEBUG("Failed send to client (%d), rc: %d\r\n", clnt->id, net_get_sock_error(clnt->io.fd));
    }
}


static lan_clnt_t *search_clnt_by_id(unsigned id)
{
    lan_clnt_t *clnt;

    list_for_each_entry(clnt, &(lan_ctrl_cb.clnt_list), self, lan_clnt_t) {
        if (clnt->id == id)
            return clnt;
    }

    return NULL;
}

