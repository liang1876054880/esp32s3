#ifndef __ARCH_FLASH_YEE_H__
#define __ARCH_FLASH_YEE_H__

#include "arch_chip.h"

void *arch_psm_flash_open(void);
int  arch_psm_flash_close(void *dev);
int  arch_psm_flash_write(uint32_t addr, uint8_t *buf, size_t len);

#endif//__ARCH_FLASH_YEE_H__
