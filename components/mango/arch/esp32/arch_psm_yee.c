#include "arch_psm.h"
#include "esp_partition.h"
#include "arch_dbg.h"
#if ETSS_PSM_ENABLE
#include "etss_psm.h"
#include "psm-v2.h"
#include "psm-internal.h"
#include "arch_psm_yee.h"
#endif

psm_hnd_t psm_handle;

#define PSM_START_ADDR  0x3F8000//ETSS_FLASH_ADDR //(44+400+400)*1024
#define PSM_SIZE        16*1024

#define FULL_VAR_NAME_SIZE 64

#define snprintf_safe( buffer,size,format,... ) ({int r__ = snprintf(buffer,size,format,##__VA_ARGS__); \
        (r__ > (size)) ? (size) : r__;})

psm_hnd_t psm_get_handle(void)
{
    return psm_handle;
}

void psm_set_handle(psm_hnd_t hnd)
{
    psm_handle = hnd;
}


int arch_psm_get_desc(flash_desc_t *f)
{
    const esp_partition_t *find_partition = NULL;
    find_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_MINVS, NULL);

    if (find_partition == NULL) {
        LOG_ERROR("No MINVS partition found!\r\n");
        f->fl_start = PSM_START_ADDR;
        f->fl_size  = PSM_SIZE;
        f->fl_dev   = 0;
    } else {
        f->fl_start = find_partition->address;
        f->fl_size  = find_partition->size;
        f->fl_dev   = 0;
    }

    //LOG_INFO("MINVS partition @ %x %d bytes\r\n", f->fl_start , f->fl_size);//etss disable log


    return 0;
}

int flash_drv_erase_sector(uint32_t start, uint32_t size)
{
    int ret = 0;
    int sector_to_erase = 0;
    int sector_count = size >> 12;
    sector_to_erase = start >> 12;
    while (sector_count--) {
        ret = spi_flash_erase_sector(sector_to_erase);
        if (ret != ETSS_OK) {
            LOG_ERROR("erase flash sector error,%s\r\n", __func__);
            return ret;
        }
        sector_to_erase ++;
    }
    return ret;
}


int etss_psm_init(unsigned int flash_address, size_t flash_size)
{
    psm_hnd_t hdl;
    flash_desc_t psm_info = {
        .fl_dev = 0,
        .fl_start = flash_address,
        .fl_size = flash_size
    };

    if (WM_SUCCESS != psm_module_init(&psm_info, &hdl, NULL)) {
        return ETSS_ERROR;
    }

    psm_set_handle(hdl);

    return ETSS_OK;
}


int etss_psm_erase_key(const char *name_space, const char *key)
{
    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(name_space) + strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, name_space);
    strcat(variable, ".");
    strcat(variable, key);

    int ret = psm_object_delete(handle, variable);

    free(variable);

    if (WM_SUCCESS != ret) {
        return ETSS_ERROR;
    }

    return ETSS_OK;
}

int etss_psm_erase_country_domain(const char *key)
{
    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, key);

    int ret = psm_object_delete(handle, variable);

    free(variable);

    if (WM_SUCCESS != ret) {
        return ETSS_ERROR;
    }

    return ETSS_OK;
}


int etss_psm_get_value(const char *name_space, const char *key, void *value, size_t length)
{
    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(name_space) + strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, name_space);
    strcat(variable, ".");
    strcat(variable, key);

    int ret = psm_get_variable(handle, variable, value, length);

    free(variable);

    if (ret < 0) {
        return ETSS_ERROR;
    }

    //ret is actural length
    return ret;
}


int etss_psm_set_value(const char *name_space, const char *key, const void *value, size_t length)
{
    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(name_space) + strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, name_space);
    strcat(variable, ".");
    strcat(variable, key);

    int ret = psm_set_variable(handle, variable, value, length);

    free(variable);

    if (WM_SUCCESS != ret) {
        return ETSS_ERROR;
    }

    return length;
}



int etss_psm_get_str(const char *name_space, const char *key, char *str, size_t str_size)
{
    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(name_space) + strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, name_space);
    strcat(variable, ".");
    strcat(variable, key);

    int ret = psm_get_variable(handle, variable, str, str_size - 1);

    free(variable);

    if (ret < 0) {
        return ETSS_ERROR;
    }

    str[ret] = '\0';

    return ret;
}

int etss_psm_get_country_domain(const char *key, char *str, size_t str_size)
{
    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, key);

    int ret = psm_get_variable(handle, variable, str, str_size - 1);

    free(variable);

    if (ret < 0) {
        return ETSS_ERROR;
    }

    str[ret] = '\0';

    return ret;
}

int etss_psm_set_str(const char *name_space, const char *key, const char *str)
{
    size_t str_len = strlen(str);

    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(name_space) + strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, name_space);
    strcat(variable, ".");
    strcat(variable, key);

    int ret = psm_set_variable(handle, variable, str, str_len);

    free(variable);

    if (WM_SUCCESS != ret) {
        return ETSS_ERROR;
    }

    return str_len;
}

int etss_psm_set_country_domain(const char *key, const char *str)
{
    size_t str_len = strlen(str);

    psm_hnd_t handle = psm_get_handle();
    if (!handle) {
        return ETSS_ERROR;
    }

    char *variable = malloc(strlen(key) + 2);
    if (!variable) {
        return ETSS_ERROR_NOMEM;
    }
    strcpy(variable, key);

    int ret = psm_set_variable(handle, variable, str, str_len);

    free(variable);

    if (WM_SUCCESS != ret) {
        return ETSS_ERROR;
    }

    return str_len;
}

