#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "driver/gpio.h"
#include "driver/spi_slave.h"
#include "nvs_flash.h"
#include "soc/rtc_periph.h"
#include "spi_slave.h"
#include "etss.h"
#include "etss_spec.h"

/* VSPI */
#define GPIO_MOSI      12
#define GPIO_MISO      13
#define GPIO_SCLK      15
#define GPIO_CS        (-1) //14
#define GPIO_HANDSHAKE 2

#define SPI_BUFFER_SIZE 128

uint8_t *sendbuf = NULL;
uint8_t *recvbuf = NULL;
spi_slave_transaction_t spi_trans = {0};

// WORD_ALIGNED_ATTR char sendbuf[129]="";
// WORD_ALIGNED_ATTR char recvbuf[129]="";
static uint32_t spi_slave_blocked_recv(uint8_t *buf);

int spi_log_hex(uint8_t *buf, int buf_len)
{
    int i = 0;
    printf("len=%d, frame: ", buf_len);
    for (i = 0; i < buf_len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\r\n");

    return 0;
}

static void spi_slave_trans_thread(void *args)
{
    uint8_t rx_buf[SPI_BUFFER_SIZE] = {0};
    uint8_t rx_buf_len = sizeof(rx_buf) / sizeof(rx_buf[0]);
    uint32_t recv_len = 0;

    while (1) {
        memset(rx_buf, 0, rx_buf_len);
        recv_len = spi_slave_blocked_recv((uint8_t *)rx_buf);
        if (0 == recv_len) {
            printf(".");
            continue;
        }

        printf("---spi slave trans thread---, recv_len: %d\r\n", recv_len);
        recv_len > 0 ? spi_log_hex(rx_buf, recv_len) : 0;

        /* TODO */
        etss_dev_data_handler(rx_buf, recv_len);
    }

    return;
}

esp_err_t spi_slave_init(void)
{
    esp_err_t ret;

    sendbuf = heap_caps_malloc(SPI_BUFFER_SIZE, MALLOC_CAP_DMA);
    recvbuf = heap_caps_malloc(SPI_BUFFER_SIZE, MALLOC_CAP_DMA);

    memset(sendbuf, 0, SPI_BUFFER_SIZE);
    memset(recvbuf, 0, SPI_BUFFER_SIZE);

    /* Configuration for the handshake line */
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_HANDSHAKE)
    };

    /* Configure handshake line as output */
    gpio_config(&io_conf);

    /* Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected. */
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    /* Configuration for the SPI bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK
    };

    /* Configuration for the SPI slave interface */
    spi_slave_interface_config_t slvcfg = {
        .mode = 3,
        .spics_io_num = GPIO_CS,
        .queue_size = 1,
        .flags = 0
    };

    /* Initialize SPI slave interface */
    ret = spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, 2);

    xTaskCreate(spi_slave_trans_thread, "spi_slave_trans_thread", 1024 * 4, NULL, 5, NULL);

    assert(ret == ESP_OK);

    return ret;
}

void spi_slave_send_signal_enable(void)
{
    WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (1ULL << GPIO_HANDSHAKE));

    LOG_INFO("emit send signal!\r\n");
}

void spi_slave_send_signal_disable(void)
{
    WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1ULL << GPIO_HANDSHAKE));
}

void spi_slave_send_buf_update(uint8_t *buf, uint8_t len)
{
    memset(sendbuf, 0, SPI_BUFFER_SIZE);
    memcpy(sendbuf, buf, len);

    LOG_DEBUG("sendbuf: ");
    len > 0 ? spi_log_hex(sendbuf, len) : 0;
}

static uint32_t spi_slave_blocked_recv(uint8_t *buf)
{
    /* clear receive buffer, set send buffer to something sane */
    memset(recvbuf, 0, SPI_BUFFER_SIZE);

    // Set up a transaction of 128 bytes to send/receive
    spi_trans.length = SPI_BUFFER_SIZE * 8;
    spi_trans.tx_buffer = sendbuf;
    spi_trans.rx_buffer = recvbuf;

    /* printf("---spi slave blocked recv init ---, addr: %d\r\n", buf); */

    spi_slave_transmit(VSPI_HOST, &spi_trans, portMAX_DELAY);

    /* spi_slave_transmit does not return until the master has done a */
    /* transmission, so by here we have sent our data and */
    /* received data from the master. Print it. */

    ;  // Transaction data length, in bits

    size_t bytes_trans_len = (spi_trans.trans_len / 8)
                             + ((spi_trans.trans_len % 8) ?  1 : 0) ;  // Transaction data length, in bits


    memcpy(buf, recvbuf, bytes_trans_len);

    /* printf("len: %d, Received: %s\n", bytes_trans_len, recvbuf); */

    /* for (int i= 0; i < bytes_trans_len; i++) { */
    /*     printf("Received len: %d, data[%d] - 0x%02x\n", */
    /*          bytes_trans_len, i, recvbuf[i]); */
    /* } */

    return bytes_trans_len;
}
