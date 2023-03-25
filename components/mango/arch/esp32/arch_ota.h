#ifndef __ARCH_OTA_H__
#define __ARCH_OTA_H__

#include "arch_flash.h"

typedef struct {
    size_t address;
    size_t size;
} ota_flash_partition_t;

typedef struct ota_flash_info {
    ota_flash_partition_t image_partitions[2];
    ota_flash_partition_t upgrade_partitions[4];
    void *ext;
} ota_flash_info_t;


int arch_ota_select_flash(ota_flash_info_t *pf, size_t ota_size);
int arch_ota_select_mcu_flash(ota_flash_info_t *pf, size_t ota_size);
int arch_ota_write_flash_with_erase(ota_flash_info_t *pf, int offset, uint8_t *pdata, size_t len);
int arch_ota_write_flash(ota_flash_info_t *pf, int offset, uint8_t *pdata, size_t len);
int arch_ota_erase_flash(ota_flash_info_t *pf, int offset, size_t len);
int arch_ota_read_flash(ota_flash_info_t *pf, int offset, uint8_t *pdata, size_t len);
int arch_ota_set_flash_boot(ota_flash_info_t *pf);
int arch_ota_check_crc(ota_flash_info_t *pf, int offset, size_t len);


#endif
