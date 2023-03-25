#ifndef __SOFT_CRC_H__
#define __SOFT_CRC_H__

#include "etss_define.h"

uint32_t soft_crc32(const void *pdata, int data_size, uint32_t crc);
uint8_t  soft_crc8(const uint8_t table[256], uint8_t *pdata, size_t nbytes, uint8_t crc);

#endif
