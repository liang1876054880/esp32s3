#ifndef __ARCH_SERIAL_H__
#define __ARCH_SERIAL_H__


#include "arch_chip.h"

#define UART_LOG            0
#define UART_COM            1

#define UART_COM_RX_PIN     10
#define UART_COM_TX_PIN     11

int serial_read(int uart_num, unsigned char *pbuf, int len, int timeout);
int serial_open(int uart_num);
void serial_write(int uart_num, const unsigned char *ptxbuf, int len);
int serial_close(int uart_num);

#endif /* __ARCH_SERIAL_H__ */

