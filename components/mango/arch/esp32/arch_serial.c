#include "arch_serial.h"
#include "arch_os.h"
#include "arch_dbg.h"

#undef ETSS_LOG_TAG
#define ETSS_LOG_TAG            "arch_serial"

void serial_write(int uart_num, const unsigned char *ptxbuf, int len)
{
    if (!ptxbuf) {
        printf("err, serial buffer is null.\r\n");
        return;
    }
    uart_write_bytes(uart_num, (const char *)ptxbuf, len);
}


int serial_read(int uart_num, unsigned char *pbuf, int len, int timeout_ms)
{
    return uart_read_bytes(uart_num, pbuf, len, ARCH_OS_WAIT_MS2TICK(timeout_ms));
}

int serial_open(int uart_num)
{
    switch (uart_num) {
    case UART_NUM_0: {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,//no flow ctrl
            .rx_flow_ctrl_thresh = 0,
        };

        //Configure UART parameters
        uart_param_config(uart_num, &uart_config);
        //Set UART pins(RTS: IO18, CTS: IO19)
        uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

        uart_driver_install(uart_num, 1024, 1024, 0, NULL, 0);
    }
    break;
    case UART_NUM_1: {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,//no flow ctrl
            .rx_flow_ctrl_thresh = 0,
        };

        //Configure UART parameters
        uart_param_config(uart_num, &uart_config);
        //Set UART pins(RTS: IO18, CTS: IO19)
        uart_set_pin(uart_num, UART_COM_TX_PIN, UART_COM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

        uart_driver_install(uart_num, 1024, 1024, 0, NULL, 0);
    }
    break;
    case UART_NUM_2:
        return -1;
        break;

    default:
        return -1;
        break;
    }

    return 0;
}

int serial_close(int uart_num)
{
    uart_driver_delete(uart_num);
    return 0;
}


