#include "arch_chip.h"
#include "arch_dbg.h"
#include "soft_crc.h"
#include "arch_flash.h"

#undef ETSS_LOG_TAG
#define ETSS_LOG_TAG            "arch_chip"

//read OTP.
/*
 *ESP32
 *B0(7 words):MAC B4-B9,CRC B10
 *B1(8 words)
 *B2(8 words)
 *B3(8 words):DID:B92-B99 KEY:B100-B115.
 *
 *
 */
//MAC+UUID+CRC.
static struct {
    uint32_t version;
    struct {
        uint8_t mac[6];
        uint8_t did[8];
        uint8_t psk[16];
        uint8_t crc8;
    } mdp;
} otp;

int arch_chip_init(void)
{
    ets_efuse_read_op();

    //get eco
    otp.version =   REG_READ(EFUSE_BLK0_RDATA3_REG);

    //get mac
    uint32_t mac_low = REG_READ(EFUSE_BLK0_RDATA1_REG);
    uint32_t mac_high = REG_READ(EFUSE_BLK0_RDATA2_REG);
    otp.mdp.mac[0] = mac_high >> 8 & 0xff;
    otp.mdp.mac[1] = mac_high >> 0 & 0xff;
    otp.mdp.mac[2] = mac_low >> 24 & 0xff;
    otp.mdp.mac[3] = mac_low >> 16 & 0xff;
    otp.mdp.mac[4] = mac_low >> 8  & 0xff;
    otp.mdp.mac[5] = mac_low >> 0  & 0xff;

    //get crc8
    uint8_t crc8_val = mac_high >> 16 & 0xff;
    otp.mdp.crc8 = crc8_val; //crc8 at last.

    //get did, in efuse is big edian
    uint32_t did_big_high, did_big_low;
    did_big_high = REG_READ(EFUSE_BLK3_RDATA0_REG);
    did_big_low = REG_READ(EFUSE_BLK3_RDATA1_REG);
    memcpy(&(otp.mdp.did[0]), &did_big_high, 4);
    memcpy(&(otp.mdp.did[4]), &did_big_low, 4);

    //get key
    uint32_t key_otp[4] = {0};
    key_otp[0] = REG_READ(EFUSE_BLK3_RDATA2_REG);
    key_otp[1] = REG_READ(EFUSE_BLK3_RDATA3_REG);
    key_otp[2] = REG_READ(EFUSE_BLK3_RDATA4_REG);
    key_otp[3] = REG_READ(EFUSE_BLK3_RDATA5_REG);
    memcpy(otp.mdp.psk, key_otp, sizeof(otp.mdp.psk));

    //crc check
    uint8_t crc_cal = soft_crc8(NULL, (uint8_t *)&otp.mdp, sizeof(otp.mdp) - 1, 0);

    if (crc_cal != otp.mdp.crc8) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "Read Efuse CRC check error!");
        goto error_exit;
    }

    //psk : all 0 and 1 check
    {
        uint32_t zero = key_otp[0]  | key_otp[1]  | key_otp[2]  | key_otp[3];
        uint32_t ffff = key_otp[0]  & key_otp[1]  & key_otp[2]  & key_otp[3];

        if (0 == zero || 0xffffffff == ffff) {
            LOG_ERROR_TAG(ETSS_LOG_TAG, "PSK check error");
            goto error_exit;
        }
    }

    return ETSS_OK;

error_exit:

    return ETSS_ERROR;
}

char *arch_get_chip_type(void)
{
    return "esp32";
}

uint32_t arch_get_chip_version(void)
{
    return otp.version;
}

void arch_get_did(uint64_t *did)
{
    *did = 0;
    for (int i = 0; i < 8; i++) {
        *did <<= 8;
        *did |= otp.mdp.did[i];
    }
}

void arch_get_psk(uint8_t psk[16])
{
    memcpy(psk, otp.mdp.psk, 16);
}

void arch_get_mac(uint8_t mac[6])
{
    memcpy(mac, otp.mdp.mac, 6);
}

int arch_get_random(uint8_t *output, size_t output_len)
{
    unsigned int random = 0;

    for (size_t i = 0, offset = 0; i < output_len; i++) {
        offset = i & 0x3;
        if (offset == 0)
            random = esp_random();

        output[i] = (random >> (offset * 8)) & 0xff;
    }

    return output_len;
}

void arch_get_flash_info(void)
{
    uint32_t flash_id = arch_get_flash_id();

    LOG_PRINT("FLASH INFO: manufacturer(0x%x), memory type(0x%x), capacity(0x%x)",
              (flash_id >> 16) & 0xFF, (flash_id >> 8) & 0xFF, flash_id & 0xFF);
}
