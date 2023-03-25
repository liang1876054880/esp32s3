#ifndef __ETSS_SPEC__
#define __ETSS_SPEC__
#include "util.h"

typedef enum {
    TYPE_CMD_REQ     = 0x20000000,
    TYPE_CMD_ACK     = 0x20000003,
    TYPE_CMD_SEND    = 0x20001001,
    TYPE_HEART_BEAT  = 0x20010000,
    TYPE_TEMPERATURE = 0x20020100,
    TYPE_CMD_BOOTING = 0x20000001,
    TYPE_QURY_DATA   = 0x20020200,
    TYPE_DEV_STATUS  = 0x20020300,
} data_type_t;

void etss_dev_data_handler(uint8_t *data, uint32_t len);
void send_data_to_CCU(data_type_t type, uint8_t *data, uint8_t data_len);

#endif

