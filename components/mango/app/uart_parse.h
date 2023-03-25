/**
 *******************************************************************************
 * @file    uart_parse.h
 * @author
 * @version
 * @date    2022-11-17
 * @brief   V1 2022-11-17
 *          create
 *
 *******************************************************************************
 * @attention
 *
 * 报文格式：
 * +--------+------+--------+--------------+----------+---------+------+
 * | 描述域 | 包头 | 操作码 | 有效载荷长度 | 有效载荷 | CRC校验 | 包尾 |
 * +--------+------+--------+--------------+----------+---------+------+
 * |  长度  |  1   |   2    |      2       |    n     |    2    |   1  |
 * +--------+------+--------+--------------+----------+---------+------+
 * |  内容  | 0x64 | opcode |     len      | payload  |   crc   | 0xee |
 * +--------+------+--------+--------------+----------+---------+------+
 *
 * *用户可自定义通讯协议
 *
 *******************************************************************************
 */

/*******************************************************************************
 ********************* define to prevent recursive inclusion *******************
 ******************************************************************************/
#ifndef __UART_PARSE__
#define __UART_PARSE__

/*******************************************************************************
 ********************************* include files *******************************
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 ************************ exported macros and struct types *********************
 ******************************************************************************/
#define HEAD            0x64
#define HEAD_SIZE       1

#define TAIL            0xEE
#define TAIL_SIZE       1

#define OPCODE_SIZE     2
#define LENGTH_SIZE     2
#define CRC_SIZE        2

#define MIN_PACKET_SIZE (HEAD_SIZE + OPCODE_SIZE + LENGTH_SIZE + CRC_SIZE + TAIL_SIZE)

#define MAX_PACKET_SIZE 256

#define MSG_MAX_PAYLOAD 64

typedef struct {
    unsigned int recv_len;
    uint8_t recv_buf[MAX_PACKET_SIZE];
} uart_context_t;

typedef int (*cmd_process_t)(uint16_t opcode, const uint8_t *data, uint32_t len);

typedef struct {
    uint16_t opcode;
    cmd_process_t cmd_process;
} method_cmd_table_t;

/*******************************************************************************
 ******************************* exported functions ****************************
 ******************************************************************************/
int method_cmd_init(const method_cmd_table_t *pcmd_table, uint32_t table_len);

void uart_recv_data(uart_context_t *ctx, const uint8_t *buf, int len);

void uart1_recv_data(const uint8_t *buf, int len);

uint8_t do_spec_data_package(uint8_t *dst_buf, int type, uint8_t *data, uint8_t data_len);

void hex_print(void *in, int len);

/*******************************************************************************
 ***************************  exported global variables ************************
 ******************************************************************************/
// extern int g_xxx;

#ifdef __cplusplus
}
#endif

#endif /* __UART_PARSE__ */

/********************************* end of file ********************************/


