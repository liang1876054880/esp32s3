#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TIME_PERIOD        (10000 *1000ULL)
#define BASE_PATH          "/spiffs"

#define STORAGE_NAMESPACE  "nvs_storage"

#define CAL_CNT_MAX  365

static const char *TAG = "storage app";
static const char *file_name = BASE_PATH "/record_data.txt";
static const char *file_name_bk = BASE_PATH "/record_data2.txt";

void spiffs_earse_all();
void write_data_overflow_handler();
int spiffs_file_size(char *file_name);
int spiffs_file_delete(char *file_name);
int spiffs_file_rename(char *file_name, char *new_file_name);

esp_err_t get_calibration_val(void);
esp_err_t set_calibration_val(void);
esp_err_t get_speed_offset(void);
esp_err_t set_speed_offset(void);

uint32_t time_tick = 0;
uint32_t time_base = 0;

uint16_t d_calibration[CAL_CNT_MAX] = {0};
int32_t c_offset = 207;

esp_vfs_spiffs_conf_t spi_conf = {
    .base_path = BASE_PATH,
    .partition_label = NULL,
    .max_files = 3,
    .format_if_mount_failed = true,
};

////////////////////////////////////////////////////////////////////////////////
//spiffs
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Init SPIFFS storage to read font file.
 *
 * @return esp_err_t Init result.
 */
esp_err_t spiffs_storage_app_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_err_t ret = esp_vfs_spiffs_register(&spi_conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(spi_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }


#if 0
    // Check consistency of reported partiton size info.
    if (used > total) {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total.  Performing SPIFFS_check().");
        ret = esp_spiffs_check(spi_conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return ret;
        } else {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
        }
    }
#endif

    return ESP_OK;
}

int spiffs_file_copy_end_half(char *src_file_name, char *dst_file_name)
{
    int file_size = spiffs_file_size(src_file_name);

    FILE *src_f = fopen(src_file_name, "rb");
    FILE *dst_f = fopen(dst_file_name, "wb");

    int pos = -(file_size / 5 * 3);
    fseek(src_f, pos, SEEK_END);
    fseek(dst_f, 0, SEEK_SET);

    char *buffer = (char *)malloc(sizeof(char) * 64);
    unsigned int index = 0;

    int read_cnt;
    while ((read_cnt = fread(buffer, sizeof(char), 64, src_f))) {
        fwrite(buffer, sizeof(char), read_cnt, dst_f);
        index++;
    }

    /* char end = '\0'; */
    /* fprintf(dst_f, &end); */

    free(buffer);

    close(src_f);
    close(dst_f);

    printf("cpy, index: %d \r\n", index);

    return 0;
}

void write_data_overflow_handler()
{
    int file_size = spiffs_file_size(file_name);

    printf("before, file_size: %d \r\n", file_size);

    if (spiffs_file_rename(file_name, file_name_bk) == 0) {
        spiffs_file_copy_end_half(file_name_bk, file_name);
    }

    file_size = spiffs_file_size(file_name);

    spiffs_file_delete(file_name_bk);

    printf("after, file_size: %d \r\n", file_size);
}

void spiffs_earse_all()
{
    if (esp_spiffs_format(spi_conf.partition_label) == ESP_OK) {
        ESP_LOGI(TAG, "esp_spiffs_format successful.....\r\n");
    } else {
        ESP_LOGI(TAG, "esp_spiffs_format failed .....\r\n");
    }
}

int spiffs_file_delete(char *file_name)
{
    struct stat st;
    if (stat(file_name, &st) == 0) {
        // Delete it if it exists
        unlink(file_name);
        return 0;
    }

    printf("Delete file[%s] error\r\n", file_name);

    return -1;
}

int spiffs_file_size(char *file_name)
{
    struct stat st;
    if (stat(file_name, &st) != 0) {
        // if file not exists
        printf("warning, file[%s] not exist\r\n", file_name);
        return -1;
    }

    return st.st_size;
}

int spiffs_file_rename(char *file_name, char *new_file_name)
{
    if (rename(file_name, new_file_name) != 0) {
        printf("rename %s  failed! \r\n", file_name);
        return -1;
    }

    return 0;
}

void test_spiffs_data_read(void)
{
    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }

    char line[64] = {0};

    /* fgets(line, sizeof(line), f); */
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    ESP_LOGI(TAG, "file size: %ld\r\n", length);
    fseek(f, 0, SEEK_SET);

    uint16_t index = 0;
    //循环读取文件的每一行数据
    while (fgets(line, 64, f) != NULL) {
        /* if (index < 50) { */
        ESP_LOGI(TAG, "Read line: %s", line);
        /* } */

        index++;
    }

    ESP_LOGI(TAG, "Read line: %s, idx: %d", line, index);

    fclose(f);
}

int spiffs_write_tail(char *data)
{
    // First create a file.
    /* ESP_LOGI(TAG, "Opening file"); */
    FILE *f = fopen(file_name, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return -1;
    }
    /* fseek(f, 0, SEEK_END); */

    /* fprintf(f, data); */
    /* fflush(f); */

    fwrite(data, sizeof(char), strlen(data), f);

    /* get file size */
    /* fseek(f, 0, SEEK_END); */
    int length = ftell(f);
    /* fseek(f, 0, SEEK_SET); */

    fclose(f);

    ESP_LOGI(TAG, "file size: %d\r\n", length);

    return length;
}

////////////////////////////////////////////////////////////////////////////////
//nvs
////////////////////////////////////////////////////////////////////////////////

void nvs_storage_app_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Updating speed offset from NVS");

    if (get_speed_offset() !=  ESP_OK) {
        set_speed_offset();
    }

    for (int i = 0; i < CAL_CNT_MAX; i++) {
        d_calibration[i] = i;
    }

    ESP_LOGI(TAG, "Updating calibration from NVS");
    if (get_calibration_val() !=  ESP_OK) {
        ESP_LOGI(TAG, "Updating calibration FALIED!!");
    }

    nvs_iterator_t it = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY);
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        it = nvs_entry_next(it);

        printf("namespace:%-15s key:%-15s type:%2d", info.namespace_name, info.key, info.type);

        printf("\n");
    };

    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);
    printf("\nCount: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d), nameSpaceCount = (%d)\n\n",
           nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries, nvs_stats.namespace_count);


}

esp_err_t set_speed_offset(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    //Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    //Write
    err = nvs_set_i32(my_handle, "speed_offset", c_offset);
    if (err != ESP_OK) {
        printf("nvs_set_i32 err !\n");
    } else {
        nvs_commit(my_handle);
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t get_speed_offset(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS");
        return err;
    }

    int32_t val = 0;
    err = nvs_get_i32(my_handle, "speed_offset", &val);

    if (err != ESP_OK) {
        printf(" nvs_get_i32 err !\n");
    } else {
        c_offset = val;
        printf("c_offset: %d\n", c_offset);
    }

    // close
    nvs_close(my_handle);

    return err;
}

esp_err_t set_calibration_val(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Write value including previously saved blob if available
    size_t required_size = sizeof(uint16_t) * CAL_CNT_MAX;
    err = nvs_set_blob(my_handle, "d_calibration", d_calibration, required_size);
    if (err != ESP_OK) {
        printf("nvs_set_blob err !\n");
    } else {
        // Commit
        nvs_commit(my_handle);
    }

    // Close
    nvs_close(my_handle);

    return ESP_OK;
}

esp_err_t get_calibration_val(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read run time blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, "d_calibration", NULL, &required_size);
    if (err != ESP_OK) {
        printf(" 1 nvs_get_blob err !\n");
    } else {
        printf("d_calibration: \n");
        if (required_size == 0) {
            printf("Nothing saved yet!\n");
        } else {
            err = nvs_get_blob(my_handle, "d_calibration", d_calibration, &required_size);
            if (err != ESP_OK) {
                printf(" 2 nvs_get_blob err !\n");
            } else {

                printf("\r\n");
                for (int i = 0; i < required_size / sizeof(uint16_t); i++) {
                    printf("%d-%d,", i, d_calibration[i]);
                }
                printf("\r\n");
            }
        }
    }

    // Close
    nvs_close(my_handle);

    return ESP_OK;
}


