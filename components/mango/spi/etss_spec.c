#include "etss_spec.h"
#include "etss.h"
#include "spi_slave.h"
#include "lcd.h"

/* boic spec */
#define HEAD               0x5a
#define END                0xa5
#define FILLED_LEN         13
#define LED_BLINK_IO_NUM   32

#define OFFSET             8

typedef struct {
    data_type_t type_ctrl;
    int (*func)(char *data);
} ctrl_method_defs_t;

static int heart_beat_ack(char *data);
static int temp_data_handler(char *data);
static int dev_booting_handler(char *data);
static uint8_t do_spec_data_packager(uint8_t *dst_buf, int type,
                                     uint8_t *data, uint8_t data_len);

static void send_buf_clear_timeout_cb(int tid, void *arg);

ctrl_method_defs_t spi_method_defs[] = {
    /* high frequency requests */
    {TYPE_HEART_BEAT, heart_beat_ack},
    {TYPE_TEMPERATURE, temp_data_handler},
    {TYPE_CMD_BOOTING, dev_booting_handler},


    /* sentinental guard */
    {0, NULL}
};

static bool data_sending = 0;
etss_tmr_t send_data_clear_tmr = {0};

static int parse_command(data_type_t type, char *data)
{
    int ret = 0;
    for (int i = 0; /* void */ ; i++) {
        if (spi_method_defs[i].func == NULL) {
            /* never here, just in case of bad code*/
            LOG_ERROR("no method found\n");
            break;
        }

        if (spi_method_defs[i].type_ctrl == type) {
            ret = spi_method_defs[i].func(data);
            break;
        }
    }

    return ret;
}

static uint16_t crc16_ibm(uint8_t *data, uint8_t length)
{
    uint8_t i;
    uint16_t crc = 0;
    while (length--) {
        /* crc ^= *data; data++; */
        crc ^= *data++;
        for (i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001; // 0xA001 = reverse 0x8005
            } else {
                crc = (crc >> 1);
            }
        }
    }
    return crc;
}

/* 大小端转换  */
bool convert_data_format(unsigned char *data, int len)
{
    int i, j;
    unsigned char tmp_char;

    i = len / 2;

    for (j = 0; j < i; j++) {
        tmp_char = data[j];
        data[j] = data[len - j - 1];
        data[len - j - 1] = tmp_char;
    }

    return true;
}

void etss_dev_data_handler(uint8_t *data, uint32_t len)
{
    static int cnt = 0;
    /* if (data[0] != 0x5a) return; */

    /* uint8_t valid_len = data[1]; */

    /* uint16_t crc = crc16_ibm(&data[1], valid_len - 4); */
    /*  */
    /* LOG_INFO("recv, crc: %x %x,  check crc: %x %x \r\n", data[valid_len - 3], */
    /*        data[valid_len - 2], (uint8_t)(crc << 8), (uint8_t)crc); */
    /*  */

    int type = data[7] << 24 | data[6] << 16 | data[5] << 8 | data [4];

    LOG_INFO("data type: 0x%04x, len: %d\r\n", type, len);

    if (data_sending && type != TYPE_CMD_ACK) return;

    parse_command(type, (char *)data);


#if 0 // only test
    etss_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type  = PERIPHERAL_TYPE_TEMP;
    memcpy(peripheral_req.data, data, len);

    ETSS_POST_REQUEST(ETSS_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(etss_peripheral_req_t), NULL, NULL);
#endif


#if 0
    switch (type) {
    case TYPE_HEART_BEAT:
        LOG_INFO("TYPE_HEART_BEAT\r\n");
        heart_beat_ack();
        break;
    case TYPE_TEMPERATURE:
        LOG_INFO("TYPE_TEMPERATURE\r\n");
        heart_beat_ack();

        temp_data_t temp_data = {0};
        memcpy(&temp_data, &data[16], 48);

        /* cnt++; */
        /* temp_data.ch0_temp += cnt; */
        /* temp_data.ch1_temp += cnt; */
        /* temp_data.ch2_temp += cnt; */
        /* temp_data.ch3_temp += cnt; */
        ETSS_POST_REQUEST(ETSS_REQ_TEMP_CHECK, &temp_data, sizeof(temp_data_t), NULL, NULL);

        break;
    case TYPE_DEV_STATUS: {
        static int cnt = 0;
        cnt++;
        LOG_INFO("TYPE_DEV_STATUS\r\n");
        heart_beat_ack();

        if (cnt % 2) {
            LCD_BLK_Clr();
        } else {
            LCD_BLK_Set();
        }
    } break;

    case TYPE_CMD_REQ:
        LOG_INFO("TYPE_CMD_REQ\r\n");
        break;
    case TYPE_CMD_ACK:
        LOG_INFO("TYPE_CMD_ACK\r\n");
        data_sending = 0;
        gpio_set_level(LED_BLINK_IO_NUM, 0);
        spi_slave_send_signal_disable();

        heart_beat_ack();
        break;
    case TYPE_QURY_DATA:
        LOG_INFO("TYPE_QURY_DATA\r\n");
        break;

    default:
        break;
    }
#endif
}

static int dev_booting_handler(char *data)
{
    uint32_t stage_len = *(uint32_t *)data - 8;
    if (stage_len < 0  || stage_len > 128) return -1;

    LOG_INFO("---> booting data[%d]:\r\n", stage_len);

    etss_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type  = PERIPHERAL_TYPE_BOOTING;
    memcpy(peripheral_req.data, data + 8, stage_len);

    ETSS_POST_REQUEST(ETSS_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(etss_peripheral_req_t), NULL, NULL);
    return 0;
}

static int temp_data_handler(char *data)
{
    uint32_t temp_stage_len = *(uint32_t *)data - 8;

    LOG_INFO("---> temp_stage data:\r\n");

    etss_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type  = PERIPHERAL_TYPE_TEMP;
    memcpy(peripheral_req.data, data + 8, temp_stage_len);

    ETSS_POST_REQUEST(ETSS_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(etss_peripheral_req_t), NULL, NULL);

#if 0
    void send_data_to_CCU(data_type_t type, uint8_t *data, uint8_t data_len);
#endif

    return 0;
}

static int heart_beat_ack(char *data)
{
    uint8_t send_data[8] = {0};
    uint8_t send_buf[OFFSET + 8] = {0};
    static int cnt = 0;

    send_data[0] = (uint8_t)(cnt >> 24);
    send_data[1] = (uint8_t)(cnt >> 16);
    send_data[2] = (uint8_t)(cnt >> 8);
    send_data[3] = (uint8_t)(cnt);
    cnt++;

    uint8_t len = do_spec_data_packager(send_buf, TYPE_HEART_BEAT, send_data, 8);

    spi_slave_send_buf_update(send_buf, len);

    return 0;
}

static void send_buf_clear_timeout_cb(int tid, void *arg)
{
    LOG_INFO("cancel send signal, fill heart beat data to send data\r\n");

    data_sending = 0;
    gpio_set_level(LED_BLINK_IO_NUM, 0);

    spi_slave_send_signal_disable();
#if 0
    heart_beat_ack(NULL);

    /* test */
    uint8_t temp[100] = {
        0x44, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x20,
        0x56, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0xD1, 0x04, 0xAB, 0x7D, 0x9C, 0x3D, 0x40,
        0x00, 0x2C, 0x52, 0x57, 0x92, 0xA9, 0x3D, 0x40,
        0xB0, 0xB7, 0x84, 0x81, 0x7F, 0x9F, 0x3B, 0x40,
        0xD0, 0x3C, 0xF4, 0x6C, 0xEB, 0xAF, 0x3B, 0x40,
        0xD0, 0x3C, 0xF4, 0x6C, 0xEB, 0xAF, 0x3B, 0x40,
        0xD0, 0x3C, 0xF4, 0x6C, 0xEB, 0xAF, 0x3B, 0x40,
    };

    etss_dev_data_handler(temp, 66);
#endif
}

void send_data_to_CCU(data_type_t type, uint8_t *data, uint8_t data_len)
{
    return;
    /* indicator */
    gpio_set_level(LED_BLINK_IO_NUM, 1);

    data_sending = 1;

    uint8_t send_buf[OFFSET + data_len];

    uint8_t len = do_spec_data_packager(send_buf, type, data, data_len);

    spi_slave_send_buf_update(send_buf, len);

    spi_slave_send_signal_enable();

    etss_start_tmr(etss_tmr_hdl,
                   &send_data_clear_tmr,
                   send_buf_clear_timeout_cb,
                   100 / MS_PER_TICK);
}

static uint8_t do_spec_data_packager(uint8_t *dst_buf, int type, uint8_t *data, uint8_t data_len)
{
    uint8_t len = 0;

    /* clear send data */
    uint8_t *pdata = dst_buf;

    /* head */
    /* *(pdata + len) = HEAD; */
    /* len++; */

    /* all data len */
    /* *(pdata + len) = data_len + FILLED_LEN; */
    /* len ++; */

    *(pdata + len) = (uint8_t)((data_len + 8));
    *(pdata + len + 1) = (uint8_t)((data_len + 8) >> 8);
    *(pdata + len + 2) = (uint8_t)((data_len + 8) >> 16);
    *(pdata + len + 3) = (uint8_t)((data_len + 8) >> 24);
    len += 4;

    /* frame total */
    /* *(pdata + len) = 0; */
    /* *(pdata + len + 1) = 1; */
    /* len += 2; */

    /* frame number */
    /* *(pdata + len) = 0; */
    /* *(pdata + len + 1) = 1; */
    /* len += 2; */

    /* data type */
    *(pdata + len) = (uint8_t)(type);
    *(pdata + len + 1) = (uint8_t)(type >> 8);
    *(pdata + len + 2) = (uint8_t)(type >> 16);
    *(pdata + len + 3) = (uint8_t)(type >> 24);
    len += 4;

    /* data */
    memcpy(pdata + len, data, data_len);
    len += data_len;

    /* crc */
    /* uint16_t  crc = crc16_ibm(pdata + 1, len - 1); */
    /* *(pdata + len) = (uint8_t)(crc << 8); */
    /* *(pdata + len + 1) = (uint8_t)crc; */
    /* len += 2; */

    /*end*/
    /* *(pdata + len) = END; */
    /* len++; */

    return len;
}

