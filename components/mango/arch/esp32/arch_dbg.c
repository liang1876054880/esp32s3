#include "arch_dbg.h"
#include "arch_os.h"

static arch_os_mutex_handle_t s_printf_mutex = NULL;

#define BEIJING_UTC_SECOND_OFFSET           8*3600
uint32_t g_gmt_offset = BEIJING_UTC_SECOND_OFFSET;

void arch_printf_begin(const char *ansi_color)
{
    struct tm date_time;
    time_t curtime;

    curtime = arch_os_utc_now() + g_gmt_offset;

    gmtime_r((const time_t *) &curtime, &date_time);


    if (s_printf_mutex)
        arch_os_mutex_get(s_printf_mutex, ARCH_OS_WAIT_FOREVER);

    if (ansi_color) {
        /* arch_printf("%s%02d:%02d:%02d.%03d ", ansi_color, date_time.tm_hour, date_time.tm_min, date_time.tm_sec, arch_os_ms_now() % 1000); */
        arch_printf("%s%02d:%02d.%03d ", ansi_color, date_time.tm_min, date_time.tm_sec, arch_os_ms_now() % 1000);
    } else {
        arch_printf("%02d:%02d:%02d.%03d ", date_time.tm_hour, date_time.tm_min, date_time.tm_sec, arch_os_ms_now() % 1000);
    }
}

void arch_printf_end(const char *ansi_color)
{
    if (ansi_color)
        arch_printf("%s\r\n", ansi_color);
    else
        arch_printf("\r\n");

    if (s_printf_mutex)
        arch_os_mutex_put(s_printf_mutex);
}

void arch_dbg_init(void)
{
    arch_os_mutex_create(&s_printf_mutex);
}

void arch_dump_hex(const void *data, int len, const char *tips)
{
    int i;
    char dump_buf[128] = {0};
    int  dump_size = 0;
    const unsigned char *data_p = data;

    arch_printf_begin(LOG_ANSI_COLOR_GRE);

    if (tips) {
        arch_printf("%s: size=%d\r\n", tips, len);
    }

    for (i = 0; i < len; i++) {
        if (0 == ((i + 1) % 16)) {
            dump_size += sprintf(dump_buf + dump_size, "0x%02x,\r\n", data_p[i]);
            dump_buf[dump_size] = '\0';
            arch_printf("%s", dump_buf);
            dump_size = 0;
            continue;
        } else {
            dump_size += sprintf(dump_buf + dump_size, "0x%02x,", data_p[i]);
        }
    }

    if (dump_size)
        arch_printf("%s", dump_buf);

    arch_printf_end(LOG_ANSI_COLOR_OFF);

    return;
}
