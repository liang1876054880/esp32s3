/**
 *******************************************************************************
 * @file    uart_app.c
 * @author  xxx
 * @version 1.0.0
 * @date    2022-12-17
 * @brief   V1 2022-12-17
 *          create
 *
 * +--------+--------------+
 * | 操作码 |    内容      |
 * +--------+--------------+
 * | 0x04A1 |   状态上报   |
 * +--------+--------------+
 * | 0x04A0 |   配置上报   |
 * +--------+--------------+
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
#include "arch_serial.h"
#include "esp_err.h"
#include "esp_log.h"
#include "uart_parse.h"

/*******************************************************************************
 * private define macro and struct types
 ******************************************************************************/
#define PER_RW_TIMEOUT_MS_MAX  (50)

#define OPCODE_UPDATE_STATE    0x04A1
#define OPCODE_UPDATE_CONFIG   0x04A0

#define ARRAY_SIZE(array)      (sizeof(array)/sizeof(*array))


/*******************************************************************************
 * private function prototypes
 ******************************************************************************/
static void send_data_to_dev(int type, uint8_t *data, uint8_t payload_len);
static int method_update_state(uint16_t opcode, const uint8_t *data, uint32_t len);
static int method_update_config(uint16_t opcode, const uint8_t *data, uint32_t len);

/*******************************************************************************
 * private variables
 ******************************************************************************/
static const char *TAG = "uart_app";
const method_cmd_table_t cmd_table[] = {
    {OPCODE_UPDATE_STATE, method_update_state},
    {OPCODE_UPDATE_CONFIG, method_update_config},
};

/*******************************************************************************
 *******************************************************************************
 * private application code, functions definitions
 *******************************************************************************
 ******************************************************************************/
int uart_cmd_blocked_recv(uint8_t *buf, int buf_size)
{
    return serial_read(UART_COM, buf, buf_size, PER_RW_TIMEOUT_MS_MAX);
}

void uart_send_data(uint8_t *buf, int buf_size)
{
    serial_write(UART_COM, buf, buf_size);
}

int bauer_cmd_blocked_recv(uint8_t *buf, int buf_size)
{
    return uart_cmd_blocked_recv(buf, buf_size);
}

int log_hex(uint8_t *buf, int buf_len)
{
    int i = 0;
    printf("len=%d, frame: ", buf_len);
    for (i = 0; i < buf_len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\r\n");

    return 0;
}

static void uart_recv_thread(void *args)
{
    uint8_t rx_buf[128] = {0};
    uint8_t rx_buf_len = sizeof(rx_buf) / sizeof(rx_buf[0]);
    uint8_t recv_len = 0;

    while (1) {
        memset(rx_buf, 0, rx_buf_len);
        recv_len = bauer_cmd_blocked_recv(rx_buf, rx_buf_len);
        if (0 == recv_len) {
            //printf(".");
            continue;
        }

        /* ESP_LOGD(TAG, "---uart thread---%d\r\n", recv_len); */
        /* recv_len > 0 ? log_hex(rx_buf, recv_len) : 0; */
        recv_len > 0 ? printf("%s \r\n", rx_buf) : 0;

        uart1_recv_data(rx_buf, recv_len);
    }

    return (void *)0;
}

int uart_recv_thread_start(void)
{
    BaseType_t task_created;
    ESP_LOGI(TAG, "create uart thread\r\n");

    task_created = xTaskCreate(uart_recv_thread,
                               "uart_events",
                               1024 * 3, NULL, 2, NULL);

    assert(task_created);
    return 0;
}

static void send_data_to_dev(int type, uint8_t *data, uint8_t payload_len)
{
    uint8_t send_buf[MIN_PACKET_SIZE + payload_len];

    uint8_t len = do_spec_data_package(send_buf, type, data, payload_len);

    uart_send_data(send_buf, len);
}

void send_state_data(void)
{
    uint8_t payload[4] = {0, 1, 2, 3};
    send_data_to_dev(OPCODE_UPDATE_CONFIG, payload, 4);
}

static int method_update_state(uint16_t opcode, const uint8_t *data, uint32_t len)
{
    printf("---> method_update_state \r\n");

    hex_print((void *)data, len);

    /* send_state_data(); */

    return 0;
}

static int method_update_config(uint16_t opcode, const uint8_t *data, uint32_t len)
{
    printf("---> method_update_config\r\n");

    hex_print((void *)data, len);

    /* send_state_data(); */

    return 0;
}

void uart_app_init(void)
{
    if (0 != serial_open(UART_COM)) {
        ESP_LOGE(TAG, "UART_COM open failed\r\n");
        return;
    }

    uart_recv_thread_start();

    method_cmd_init(cmd_table, ARRAY_SIZE(cmd_table));
}

/********************************* end of file ********************************/

