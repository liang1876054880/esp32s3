
#include "arch_crash.h"
#include "arch_flash.h"
#include "arch_dbg.h"

#undef ETSS_LOG_TAG
#define ETSS_LOG_TAG            "arch_crash"

#define COREDUMP_FLASH_MAGIC_START    0xE32C04EDUL
#define COREDUMP_FLASH_MAGIC_END      0xE32C04EDUL

bool arch_crash_exist(void)
{
    const esp_partition_t *coredump_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);

    if (coredump_partition) {

        LOG_DEBUG_TAG(ETSS_LOG_TAG, "partition '%s' @ %x %d bytes", coredump_partition->label, coredump_partition->address, coredump_partition->size);

        //read coredump magic and compare
        uint32_t magic_start;
        arch_flash_read(coredump_partition->address, (uint8_t *)&magic_start, sizeof(uint32_t));

        if (COREDUMP_FLASH_MAGIC_START == magic_start) {

            LOG_WARN_TAG(ETSS_LOG_TAG, "crash found");

            return true;
        }
    }

    return false;
}


void arch_crash_delete(void)
{
    const esp_partition_t *coredump_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);

    if (coredump_partition) {

        LOG_DEBUG_TAG(ETSS_LOG_TAG, "partition '%s' @ %x %d bytes", coredump_partition->label, coredump_partition->address, coredump_partition->size);

        //read coredump magic and compare
        uint32_t magic_start;
        arch_flash_read(coredump_partition->address, (uint8_t *)&magic_start, sizeof(uint32_t));

        if (COREDUMP_FLASH_MAGIC_START == magic_start) {

            arch_flash_erase(coredump_partition->address);

            LOG_DEBUG_TAG(ETSS_LOG_TAG, "crash erased");
        }
    }
}
