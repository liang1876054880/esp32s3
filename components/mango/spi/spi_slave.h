#ifndef __SPI_SLAVE__
#define __SPI_SLAVE__

#include "esp_event.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"

esp_err_t spi_slave_init(void);
void spi_slave_send_buf_update(uint8_t *buf, uint8_t len);
void spi_slave_send_signal_disable(void);
void spi_slave_send_signal_enable(void);

#endif





