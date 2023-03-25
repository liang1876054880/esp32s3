/**
 *******************************************************************************
 * @file    uart_parse.c
 * @author  xxx
 * @version 1.0.0
 * @date    2022-11-17
 * @brief   V1 2022-11-17
 *          create
 *
 *          V2 2022-11-24
 *          1. 修复缓存剩余数据时，
 *             使用指针(PAYLOAD_LEN)计算payload长度导致recv_len异常的问题
 *
 *          2. 添加数据打包函数
 *
 *          V3 2022-11-25
 *          1. 添加操作命令解析函数 method_cmd_table_t
 *
 *******************************************************************************
 * @attention
 *
 *
 *******************************************************************************
 */

/*******************************************************************************
 * include files
 ******************************************************************************/
#include <string.h>
#include "uart_parse.h"

#include "arch_dbg.h"

/*******************************************************************************
 * private define macro and struct types
 ******************************************************************************/
/* #define LOG_INFO    LOGI */
/* #define LOG_DEBUG   LOGD */

#define MIN(a, b)       (((a) < (b)) ? (a) : (b))

#define BYTE0(x)        ((uint8_t)( (x) & 0x000000ff))
#define BYTE1(x)        ((uint8_t)(((x) & 0x0000ff00) >>  8))
#define BYTE2(x)        ((uint8_t)(((x) & 0x00ff0000) >> 16))
#define BYTE3(x)        ((uint8_t)(((x) & 0xff000000) >> 24))

#define HEAD_DEF        {BYTE0(HEAD)}
#define TAIL_DEF        {BYTE0(TAIL)}

#define HEADER_PTR      (&(ctx->recv_buf[0]))
#define PACKET_HEAD     HEADER_PTR[0]
#define OPCODE_PTR      (HEADER_PTR + HEAD_SIZE)
#define LENGTH_PTR      (OPCODE_PTR + OPCODE_SIZE)
#define PAYLOAD_LEN     ((uint16_t)(LENGTH_PTR[0] << 8) | (uint16_t)LENGTH_PTR[1])
#define PAYLOAD_PTR     (LENGTH_PTR + LENGTH_SIZE)

#define CRC_PTR         (PAYLOAD_PTR + PAYLOAD_LEN)
#define CRC_VAL         ((uint16_t)(CRC_PTR[0] << 8) | (uint16_t)CRC_PTR[1])
#define TAIL_PTR        (CRC_PTR + CRC_SIZE)
#define PACKET_TAIL     TAIL_PTR[0]

#define PACKET_LEN      (HEAD_SIZE + OPCODE_SIZE + LENGTH_SIZE + PAYLOAD_LEN + CRC_SIZE + TAIL_SIZE)

/*******************************************************************************
 * private function prototypes
 ******************************************************************************/
static uint16_t crc16_ibm(uint8_t *data, uint16_t length);

void do_spec_data_process(uint8_t *buf, int len);

/*******************************************************************************
 * private variables
 ******************************************************************************/
uart_context_t uart_ctx = {0};

static const method_cmd_table_t *cmd_table = NULL;
static uint32_t cmd_table_len = 0;
/*******************************************************************************
 * external variables and functions
 ******************************************************************************/
/* extern const method_cmd_table_t cmd_table[]; */

/*******************************************************************************
 *******************************************************************************
 * private application code, functions definitions
 *******************************************************************************
 ******************************************************************************/
void uart1_recv_data(const uint8_t *buf, int len)
{
    uart_recv_data(&uart_ctx, buf, len);
}

void uart_recv_data(uart_context_t *ctx, const uint8_t *buf, int len)
{
    uint8_t head[HEAD_SIZE] = HEAD_DEF;

    int off_head = 0;
    int copy_len = 0;

copy_data:

    if (len == 0) {
        LOG_DEBUG("no available data, return!\n");
        return;
    }

    copy_len = MIN(sizeof(ctx->recv_buf) - ctx->recv_len, len);
    if (copy_len) {
        memcpy(ctx->recv_buf + ctx->recv_len, buf, copy_len);
        buf += copy_len;
        len -= copy_len;
        ctx->recv_len += copy_len;

        LOG_DEBUG("--->copy_len: %d, recv_len: %d\n", copy_len, ctx->recv_len);
    }

find_head:

    if (ctx->recv_len < MIN_PACKET_SIZE) {
        LOG_DEBUG("local buffer size[%d] too few, get more\n", ctx->recv_len);
        /* hex_print(HEADER_PTR, ctx->recv_len); */

        goto copy_data;
    }

    off_head = 0;

    while (off_head <= ctx->recv_len - MIN_PACKET_SIZE &&
            memcmp(ctx->recv_buf + off_head, head, sizeof(head))) {
        off_head ++;
        LOG_DEBUG("off_head: %d, recv_len: %d, min_size: %d\n", off_head, ctx->recv_len, MIN_PACKET_SIZE);
    }

    if (off_head) {
        LOG_DEBUG("dropping all bytes before head match\n");
        memmove(ctx->recv_buf, ctx->recv_buf + off_head, ctx->recv_len - off_head);
        ctx->recv_len -= off_head;
    }

    if (off_head > ctx->recv_len - MIN_PACKET_SIZE) {
        LOG_DEBUG("available data too few, goto copy..\r\n");
        goto copy_data;
    }

    LOG_DEBUG("PACKET_LEN: %d, MSG_MAX_PAYLOAD: %d\n", PACKET_LEN, MSG_MAX_PAYLOAD);
    /* head and length */
    if (PACKET_HEAD != HEAD || PAYLOAD_LEN > MSG_MAX_PAYLOAD) {
        LOG_DEBUG("invalid header[%x] or length[%d], drop head\n",
                  PACKET_HEAD, PAYLOAD_LEN);

        memmove(ctx->recv_buf, ctx->recv_buf + HEAD_SIZE, ctx->recv_len - HEAD_SIZE);

        ctx->recv_len -= HEAD_SIZE;

        goto find_head;
    }

    /* PACKET_LEN */
    if (ctx->recv_len < PACKET_LEN) {

        if (ctx->recv_len == sizeof(ctx->recv_buf)) {

            LOG_DEBUG("message size exceeds receive buffer size, corrupted?\n");

            memmove(ctx->recv_buf, ctx->recv_buf + HEAD_SIZE, ctx->recv_len - HEAD_SIZE);

            ctx->recv_len -= HEAD_SIZE;

            goto find_head;

        } else {
            LOG_DEBUG("recv_len < PACKET_LEN, goto copy..\r\n");
            goto copy_data;
        }
    }

    /* tail */
    if (PACKET_TAIL != TAIL) {
        LOG_DEBUG("invalid tail[%x], drop head, find head again!\n", PACKET_TAIL);

        memmove(ctx->recv_buf, ctx->recv_buf + HEAD_SIZE, ctx->recv_len - HEAD_SIZE);

        ctx->recv_len -= HEAD_SIZE;

        goto find_head;
    }

    /* crc */
    uint16_t calc_crc = crc16_ibm(HEADER_PTR, HEAD_SIZE + OPCODE_SIZE + LENGTH_SIZE + PAYLOAD_LEN);
    if (CRC_VAL != calc_crc) {
        LOG_DEBUG("invalid crc[%02x - %02x], drop head, find head again\n", calc_crc, CRC_VAL);

        memmove(ctx->recv_buf, ctx->recv_buf + HEAD_SIZE, ctx->recv_len - HEAD_SIZE);

        ctx->recv_len -= HEAD_SIZE;

        goto find_head;
    }

    uint32_t msg_len = PACKET_LEN;
    /* LOG_DEBUG("msg_len: %d, recv_len: %d\n", msg_len, ctx->recv_len); */

    do_spec_data_process(HEADER_PTR, msg_len);

    /* cache remaining data */
    if (ctx->recv_len - msg_len) {
        memmove(ctx->recv_buf, ctx->recv_buf + msg_len, ctx->recv_len - msg_len);
    }

    ctx->recv_len -= msg_len;
    LOG_DEBUG("retry goto find head, msg len: %d, recv_len: %d\n", msg_len, ctx->recv_len);

    goto find_head;
}

int method_cmd_init(const method_cmd_table_t *pcmd_table, uint32_t table_len)
{
    cmd_table = pcmd_table;
    cmd_table_len = table_len;

    return 0;
}

void do_spec_data_process(uint8_t *buf, int len)
{
#define _OPCODE_PTR   (&(buf[0]) + HEAD_SIZE)
#define _OPCODE_VAL   ((uint16_t)(_OPCODE_PTR[0] << 8) | (uint16_t)_OPCODE_PTR[1])
#define _LENGTH_PTR   (_OPCODE_PTR + OPCODE_SIZE)
#define _PAYLOAD_LEN  ((uint16_t)(_LENGTH_PTR[0] << 8) | (uint16_t)_LENGTH_PTR[1])
#define _PAYLOAD_PTR  (_LENGTH_PTR + LENGTH_SIZE)

    LOG_INFO("------> [esp] do_spec_data_process: ");
    hex_print(buf, len);
    LOG_INFO("opcode: 0x%04x, len: %d \r\n ", _OPCODE_VAL, _PAYLOAD_LEN);

    if (NULL == cmd_table) {
        return;
    }

    for (uint16_t i = 0; i < cmd_table_len; ++i) {
        if (cmd_table[i].opcode == _OPCODE_VAL) {
            if (cmd_table[i].cmd_process) {
                cmd_table[i].cmd_process(_OPCODE_VAL, _PAYLOAD_PTR, _PAYLOAD_LEN);
            }
        }
    }

    return;
}

#if 0
void send_data_to_dev(int type, uint8_t *data, uint8_t payload_len)
{
    uint8_t send_buf[MIN_PACKET_SIZE + payload_len];

    uint8_t len = do_spec_data_package(send_buf, type, data, payload_len);

    LOG_INFO(">>> send: \r\n");
    hex_print(send_buf, len);

    /* uart_data_send(send_buf, len); */
}
#endif

uint8_t do_spec_data_package(uint8_t *dst_buf, int type, uint8_t *data, uint8_t data_len)
{
    uint8_t len = 0;

    /* clear send data */
    uint8_t *pdata = dst_buf;

    /* head */
    *(pdata + len) = HEAD;
    len++;

    /* data type */
    *(pdata + len) = BYTE1(type);
    *(pdata + len + 1) = BYTE0(type);
    len += 2;

    /* payload len*/
    *(pdata + len) = BYTE1(data_len);
    *(pdata + len + 1) = BYTE0(data_len);
    len += 2;

    /* payload */
    memcpy(pdata + len, data, data_len);
    len += data_len;

    /* crc */
    uint16_t  crc = crc16_ibm(pdata, len);
    *(pdata + len) = BYTE1(crc);
    *(pdata + len + 1) = BYTE0(crc);
    len += 2;

    /*end*/
    *(pdata + len) = TAIL;
    len++;

    return len;
}

static uint16_t crc16_ibm(uint8_t *data, uint16_t length)
{
    uint8_t i;
    uint16_t crc = 0;

    while (length--) {
        /* crc ^= *data; data++; */
        crc ^= *data++;
        for (i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;  // 0xA001 = reverse 0x8005
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return crc;
}

void hex_print(void *in, int len)
{
    static char hex_buf[64];
    int i;
    char tmp[4] = {0};
    uint8_t *data = (uint8_t *)in;
    memset(hex_buf, 0x00, sizeof(hex_buf));
    for (i = 0; i < len; i++) {
        if (i % 16 == 0 && i) {
            printf("%s", hex_buf);
            memset(hex_buf, 0x00, sizeof(hex_buf));
        }
        sprintf(tmp, "%02x ", data[i]);
        strcat(hex_buf, tmp);
    }
    printf("%s", hex_buf);
    printf("\r\n");
}


/********************************* end of file ********************************/

