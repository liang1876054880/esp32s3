/*******************************************************************************
Filename:       arch_flash.c
Description:    This file contains the ESP flash API wrappers
 *******************************************************************************/


#include "arch_flash.h"
#include "arch_dbg.h"

#define FLASH_ADDR_2_SECTOR_INDEX(addr)      (addr>>12)

int arch_flash_erase(uint32_t addr)
{
    int ret = spi_flash_erase_sector(FLASH_ADDR_2_SECTOR_INDEX(addr));

    return (ESP_OK == ret) ? ETSS_OK : ETSS_ERROR;
}

int arch_flash_erase_sector(uint32_t sector)
{
    int ret = spi_flash_erase_sector(sector);

    return (ESP_OK == ret) ? ETSS_OK : ETSS_ERROR;
}

//no erase,need to erase first.
int arch_flash_write(uint32_t addr, uint8_t *buf, size_t len)
{
    uint32_t temp;
    uint8_t *tempBuf = (uint8_t *)&temp;
    uint32_t baseAddr = addr;
    uint8_t *p = buf;
    uint8_t *newBuf = NULL;
    size_t writeLen = len;
    uint32_t offset = 0;
    size_t last = 0;

    if (0 == writeLen) {
        return ETSS_OK;
    }

    //addr is not aligned
    if (addr & 0x03) {
        //LOG_WARN("%s: addr not aligned.", __FUNCTION__);
        baseAddr = addr & 0xFFFFFFFC;
        offset = addr & 0x03;
        last = 4 - offset;
        if (writeLen < last) {
            last = writeLen;
        }
        writeLen -= last;
        spi_flash_read(baseAddr, tempBuf, 4);
        while (last > 0) {
            tempBuf[offset++] = *p++;
            last--;
        }
        spi_flash_write(baseAddr, tempBuf, 4);
        baseAddr += 4;
    }

    if (0 == writeLen) {
        return ETSS_OK;
    }

#ifdef ESP_PLATFORM
    /* On the ESP32, data can come from DROM, which is not accessible by spi_flash_write
     * function. To work around this, we copy the data to heap if it came from DROM.
     * Hopefully this won't happen very often in practice. For data from DRAM, we should
     * still be able to write it to flash directly.
     * TODO: figure out how to make this platform-specific check nicer (probably by introducing
     * a platform-specific flash layer).
     */
    if ((uint32_t) p < 0x3ff00000) {
        LOG_WARN_TAG("arch_flash", "data from DROM.");
        newBuf = (uint8_t *) malloc(writeLen);
        if (!newBuf) {
            LOG_ERROR_TAG("arch_flash", "no mem.");
            return ETSS_ERROR_NOMEM;
        }
        memcpy(newBuf, p, writeLen);
        p = newBuf;
    }
#endif //ESP_PLATFORM

    /* Write blocks */
    last = writeLen - (writeLen & 0x03);
    if (((uint32_t)p) & 0x03) {
        //非4bytes对齐
        //LOG_WARN("%s: mem not aligned.", __FUNCTION__);
        while (last > 0) {
            tempBuf[0] = *p++;
            tempBuf[1] = *p++;
            tempBuf[2] = *p++;
            tempBuf[3] = *p++;
            spi_flash_write(baseAddr, tempBuf, 4);
            last -= 4;
            baseAddr += 4;
        }
    } else if (last > 0) {
        spi_flash_write(baseAddr, p, last);
        p += last;
        baseAddr += last;
    }

    /* Write last */
    last = writeLen & 0x03;
    if (last > 0) {
        memset(tempBuf, 0xff, 4);
        offset = 0;
        while (last > 0) {
            tempBuf[offset++] = *p++;
            last--;
        }
        spi_flash_write(baseAddr, tempBuf, 4);
        baseAddr += 4;
    }

    if (newBuf) {
        free((void *)newBuf);
        newBuf = NULL;
    }

    return ETSS_OK;
}

int arch_flash_read(uint32_t addr, uint8_t *buf, size_t len)
{
    uint32_t temp;
    uint8_t *tempBuf = (uint8_t *)&temp;
    uint32_t baseAddr = addr;
    uint8_t *p = buf;
    size_t readLen = len;
    uint32_t offset = 0;
    size_t last = 0;

    if (0 == readLen) {
        return ETSS_OK;
    }

    //addr is not aligned
    if (addr & 0x03) {
        //LOG_WARN("%s: addr not aligned.", __FUNCTION__);
        baseAddr = addr & 0xFFFFFFFC;
        offset = addr & 0x03;
        last = 4 - offset;
        if (readLen < last) {
            last = readLen;
        }
        readLen -= last;
        spi_flash_read(baseAddr, tempBuf, 4);
        while (last > 0) {
            *p++ = tempBuf[offset++];
            last--;
        }
        baseAddr += 4;
    }

    if (0 == readLen) {
        return ETSS_OK;
    }

    /* Read blocks */
    last = readLen - (readLen & 0x03);
    if (((uint32_t)p) & 0x03) {
        //非4bytes对齐
        //LOG_WARN("%s: mem not aligned.", __FUNCTION__);
        while (last > 0) {
            spi_flash_read(baseAddr, tempBuf, 4);
            *p++ = tempBuf[0];
            *p++ = tempBuf[1];
            *p++ = tempBuf[2];
            *p++ = tempBuf[3];
            last -= 4;
            baseAddr += 4;
        }
    } else if (last > 0) {
        spi_flash_read(baseAddr, p, last);
        p += last;
        baseAddr += last;
    }

    /* Read last */
    last = readLen & 0x03;
    if (last > 0) {
        offset = 0;
        spi_flash_read(baseAddr, tempBuf, 4);
        while (last > 0) {
            *p++ = tempBuf[offset++];
            last--;
        }
        baseAddr += 4;
    }

    return ETSS_OK;
}

uint32_t arch_get_flash_id(void)
{
    return g_rom_flashchip.device_id;
}
