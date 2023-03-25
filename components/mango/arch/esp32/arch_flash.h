
/*******************************************************************************
  Filename:       arch_flash.h
  Description:    This file contains the arch flash API definitions and prototypes.
*******************************************************************************/

#ifndef __ARCH_FLASH_H__
#define __ARCH_FLASH_H__

#include "arch_chip.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */


/*********************************************************************
 * CONSTANTS
 */


#define FLASH_SECTOR_SIZE                   SPI_FLASH_SEC_SIZE

#define ESP_FLASH_SECTOR_INDEX(addr)        (0x1C0000 >>0x1000)

/*
 * @brief ETSS FLASH Region
 */
//4M Flash layout.
//
/*
 * partition table
    # xiaomi flash layout.first fw addr 0x10000, second fw addr 0x170000,factory fw can load at second fw.
    # must align to 64KB, If you want to change flash layout,just mod it.partition table file name is xiaomi_partitions_4M.csv
    ## Label            Usage          Type ST Offset   Length
    0 nvs              WiFi data        01 02 00009000 00004000
    1 otadata          OTA data         01 00 0000d000 00002000
    2 phy_init         RF data          01 01 0000f000 00001000
    3 etss_fw1         OTA app          00 10 00010000 00160000
    4 etss_fw2         OTA app          00 11 00170000 00160000
    5 test             test app         00 20 002d0000 00013000
    6 mimcu            Unknown data     01 fd 002e3000 00100000
    7 coredump         Unknown data     01 03 003e3000 00010000
    8 minvs            Unknown data     01 fe 003f8000 00004000
 *
 *注意：APP fw的起始地址必须是64K对齐.
 */


//void arch_flash_init( void );
int arch_flash_erase(uint32_t addr);
int arch_flash_erase_sector(uint32_t sector);
int arch_flash_read(uint32_t addr, uint8_t *buf, size_t len);
int arch_flash_write(uint32_t addr, uint8_t *buf, size_t len);
uint32_t arch_get_flash_id(void);

#ifdef __cplusplus
}
#endif

#endif /* __ARCH_FLASH_H__ */




