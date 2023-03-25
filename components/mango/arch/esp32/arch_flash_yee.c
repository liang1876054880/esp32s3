#include "arch_flash_yee.h"
#include "arch_flash.h"

// only for psm
void *arch_psm_flash_open(void)
{
    return (void *)1;
}

int arch_psm_flash_close(void *dev)
{
    return ETSS_OK;
}

int arch_psm_flash_write(uint32_t addr, uint8_t *buf, size_t len)
{
    //LOG_DEBUG("bill debug arch_psm_flash_write, addr = 0x%x, len = %d\r\n", addr, len);
    return arch_flash_write(addr, buf, len);
}

