#include "arch_ota.h"
#include "arch_flash.h"
#include "arch_dbg.h"
#include "soft_crc.h"
#include "util.h"

#undef ETSS_LOG_TAG
#define ETSS_LOG_TAG            "arch_ota"

int arch_ota_select_flash(ota_flash_info_t *pf, size_t ota_size)
{
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (configured != running) {
        LOG_WARN_TAG(ETSS_LOG_TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
        LOG_WARN_TAG(ETSS_LOG_TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    LOG_DEBUG_TAG(ETSS_LOG_TAG, "Running partition type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);

    const esp_partition_t *ota_0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    const esp_partition_t *ota_1_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);

    if (ota_0_partition) {
        pf->image_partitions[0].address = ota_0_partition->address;
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "OTA_0 partition type %d subtype %d (offset 0x%08x)", ota_0_partition->type, ota_0_partition->subtype, ota_0_partition->address);
    }

    if (ota_1_partition) {
        pf->image_partitions[1].address = ota_1_partition->address;
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "OTA_1 partition type %d subtype %d (offset 0x%08x)", ota_1_partition->type, ota_1_partition->subtype, ota_1_partition->address);
    }

    const esp_partition_t *update_partition = NULL;
    int upgrade_partitions_index = 0;
    size_t upgrede_partitions_size = 0;
    memset(pf->upgrade_partitions, 0, sizeof(pf->upgrade_partitions));

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition) {
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "Update partition type %d subtype %d (offset 0x%08x)", update_partition->type, update_partition->subtype, update_partition->address);
        pf->upgrade_partitions[upgrade_partitions_index].address = update_partition->address;;
        pf->upgrade_partitions[upgrade_partitions_index].size = update_partition->size;
        upgrede_partitions_size += update_partition->size;
        upgrade_partitions_index++;
    }

    if (upgrade_partitions_index < NELEMENTS(pf->upgrade_partitions)) {
        pf->upgrade_partitions[upgrade_partitions_index].address = 0;
        pf->upgrade_partitions[upgrade_partitions_index].size = 0;
    }

    pf->ext = (void *)update_partition;

    if (ota_size > upgrede_partitions_size) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "ota size[%x] is larger than partition size[%x]", ota_size, pf->upgrade_partitions[0].size);
        return -1;
    }

    return 0;
}

int arch_ota_select_mcu_flash(ota_flash_info_t *pf, size_t ota_size)
{
    const esp_partition_t *mcu_partition = NULL;
    int upgrade_partitions_index = 0;
    size_t upgrade_partitions_size = 0;
    memset(pf->upgrade_partitions, 0, sizeof(pf->upgrade_partitions));

    mcu_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_MIMCU, NULL);
    if (mcu_partition) {
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "MIMCU partition[%d] @ %x %d bytes", upgrade_partitions_index, mcu_partition->address, mcu_partition->size);
        pf->upgrade_partitions[upgrade_partitions_index].address = mcu_partition->address;
        pf->upgrade_partitions[upgrade_partitions_index].size = mcu_partition->size;
        upgrade_partitions_size += mcu_partition->size;
        upgrade_partitions_index++;
    }

    /*
     * find other partitions
     mcu_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_MIMCU, NULL);
     if(mcu_partition){
     LOG_INFO_TAG(ETSS_LOG_TAG, "MIMCU partition[%d] @ %x %d bytes", upgrade_partitions_index, mcu_partition->address , mcu_partition->size);
     pf->upgrade_partitions[upgrade_partitions_index].address = mcu_partition->address;
     pf->upgrade_partitions[upgrade_partitions_index].size = mcu_partition->size;
     upgrade_partitions_size += mcu_partition->size;
     upgrade_partitions_index++;
     }
     */

    if (upgrade_partitions_index < NELEMENTS(pf->upgrade_partitions)) {
        pf->upgrade_partitions[upgrade_partitions_index].address = 0;
        pf->upgrade_partitions[upgrade_partitions_index].size = 0;
    }

    if (ota_size > upgrade_partitions_size) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "ota size[%x] is larger than partition size[%x]", ota_size, upgrade_partitions_size);
        return -1;
    }

    return 0;
}


static ota_flash_partition_t *find_upgrade_partition(ota_flash_info_t *pf, size_t offset, size_t *partition_offset)
{
    ota_flash_partition_t *upgrade_partition = NULL;
    size_t upgrade_partition_offset = 0;
    size_t upgrade_partitions_size = 0;

    for (int i = 0; i < NELEMENTS(pf->upgrade_partitions); i++) {
        if (0 == pf->upgrade_partitions[i].size) {
            break;
        }
        if (offset < (upgrade_partitions_size + pf->upgrade_partitions[i].size)) {
            upgrade_partition_offset = offset - upgrade_partitions_size;
            upgrade_partition = &(pf->upgrade_partitions[i]);
            break;
        }
        upgrade_partitions_size += pf->upgrade_partitions[i].size;
    }

    if (partition_offset) {
        *partition_offset = upgrade_partition_offset;
    }

    return upgrade_partition;
}

int arch_ota_write_flash_with_erase(ota_flash_info_t *pf, int offset, uint8_t *pdata, size_t len)
{
    while (len > 0) {
        size_t upgrade_partition_offset = 0;
        ota_flash_partition_t *upgrade_partition = find_upgrade_partition(pf, offset, &upgrade_partition_offset);
        if (NULL == upgrade_partition) {
            return ETSS_ERROR_SIZE;
        }

        size_t w_len = MIN(len, upgrade_partition->size - upgrade_partition_offset);
        size_t w_addr = upgrade_partition->address + upgrade_partition_offset;

        {
            //erase
            static uint32_t  last_erased_sector_addr = 0xFFFFFFFF;//写之前会擦除相应的flash sector，但是每次写的数据可能小于one sector，该变量避免误擦

            size_t erase_sector_addr = w_addr & ~(FLASH_SECTOR_SIZE - 1);
            size_t e_addr = w_addr + w_len;

            while (erase_sector_addr < e_addr) {
                if (erase_sector_addr != last_erased_sector_addr) {
                    if (ETSS_OK != arch_flash_erase(erase_sector_addr)) {
                        return ETSS_ERROR;
                    }
                    last_erased_sector_addr = erase_sector_addr;
                }
                erase_sector_addr += FLASH_SECTOR_SIZE;
            }
        }

        //write
        if (ETSS_OK != arch_flash_write(w_addr, pdata, w_len)) {
            return ETSS_ERROR;
        }

        pdata  += w_len;
        len    -= w_len;
        offset += w_len;
    }

    return ETSS_OK;
}

int arch_ota_write_flash(ota_flash_info_t *pf, int offset, uint8_t *pdata, size_t len)
{
    while (len > 0) {
        size_t upgrade_partition_offset = 0;
        ota_flash_partition_t *upgrade_partition = find_upgrade_partition(pf, offset, &upgrade_partition_offset);
        if (NULL == upgrade_partition) {
            return ETSS_ERROR_SIZE;
        }

        size_t w_len = MIN(len, upgrade_partition->size - upgrade_partition_offset);
        size_t w_addr = upgrade_partition->address + upgrade_partition_offset;

        //write
        if (ETSS_OK != arch_flash_write(w_addr, pdata, w_len)) {
            return ETSS_ERROR;
        }

        pdata  += w_len;
        len    -= w_len;
        offset += w_len;
    }

    return ETSS_OK;
}

//仅仅作为擦除flash magic使用，没有做防止误擦处理，请勿用于其他用途。
int arch_ota_erase_flash(ota_flash_info_t *pf, int offset, size_t len)
{
    while (len > 0) {
        size_t upgrade_partition_offset = 0;
        ota_flash_partition_t *upgrade_partition = find_upgrade_partition(pf, offset, &upgrade_partition_offset);
        if (NULL == upgrade_partition) {
            return ETSS_ERROR_SIZE;
        }

        size_t w_len = MIN(len, upgrade_partition->size - upgrade_partition_offset);
        size_t w_addr = upgrade_partition->address + upgrade_partition_offset;

        size_t erase_sector_addr = w_addr & ~(FLASH_SECTOR_SIZE - 1);
        size_t e_addr = w_addr + w_len;

        while (erase_sector_addr < e_addr) {
            if (ETSS_OK != arch_flash_erase(erase_sector_addr)) {
                return ETSS_ERROR;
            }
            erase_sector_addr += FLASH_SECTOR_SIZE;
        }

        len -= w_len;
    }

    return ETSS_OK;
}


int arch_ota_read_flash(ota_flash_info_t *pf, int offset, uint8_t *pdata, size_t len)
{
    while (len > 0) {
        size_t upgrade_partition_offset = 0;
        ota_flash_partition_t *upgrade_partition = find_upgrade_partition(pf, offset, &upgrade_partition_offset);
        if (NULL == upgrade_partition) {
            return ETSS_ERROR_SIZE;
        }

        size_t r_len = MIN(len, upgrade_partition->size - upgrade_partition_offset);
        size_t r_addr = upgrade_partition->address + upgrade_partition_offset;

        //read
        if (ETSS_OK != arch_flash_read(r_addr, pdata, r_len)) {
            return ETSS_ERROR;
        }

        pdata  += r_len;
        len    -= r_len;
        offset += r_len;
    }

    return ETSS_OK;
}


int arch_ota_set_flash_boot(ota_flash_info_t *pf)
{
    const esp_partition_t *update_partition = (const esp_partition_t *)pf->ext;

    int ret = esp_ota_set_boot_partition(update_partition);
    if (ETSS_OK != ret) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "Set ota partition table error %d ", ret);
        return ETSS_ERROR;
    }

    return ETSS_OK;
}

int arch_ota_check_crc(ota_flash_info_t *pf, int offset, size_t len)
{
    uint32_t crc32_fw = 0;
    uint32_t crc32 = 0;

    if (len <= 4) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "len check failed");
        return ETSS_ERROR;
    }

    if (ETSS_OK != arch_ota_read_flash(pf, offset + len - 4, (uint8_t *)&crc32_fw, 4)) {
        return ETSS_ERROR;
    }

    len -= 4;
    while (len > 0) {
        size_t upgrade_partition_offset = 0;
        ota_flash_partition_t *upgrade_partition = find_upgrade_partition(pf, offset, &upgrade_partition_offset);
        if (NULL == upgrade_partition) {
            return ETSS_ERROR_SIZE;
        }

        size_t r_len = MIN(len, upgrade_partition->size - upgrade_partition_offset);
        size_t r_addr = upgrade_partition->address + upgrade_partition_offset;
        size_t r_offset = 0;

        {
            //crc cal
            uint32_t dummy_buf[32];
            uint32_t blk = 0;

            while (r_offset < r_len) {
                blk = MIN(r_len - r_offset, sizeof(dummy_buf));
                arch_flash_read(r_addr + r_offset, (uint8_t *)dummy_buf, blk);
                crc32 = soft_crc32(dummy_buf, blk, crc32);
                r_offset += blk;
            }
        }

        len    -= r_len;
        offset += r_len;
    }

    if (crc32 != crc32_fw) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "crc check failed! ");
        return ETSS_ERROR;
    }

    LOG_DEBUG_TAG(ETSS_LOG_TAG, "crc check passed");

    return ETSS_OK;
}
