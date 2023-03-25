#include "arch_time.h"

static uint32_t SEC_PER_YR[2] = { 31536000, 31622400 };
static uint32_t SEC_PER_MT[2][12] =  {
    {
        2678400, 2419200, 2678400, 2592000, 2678400, 2592000,
        2678400, 2678400, 2592000, 2678400, 2592000, 2678400
    },
    {
        2678400, 2505600, 2678400, 2592000, 2678400, 2592000,
        2678400, 2678400, 2592000, 2678400, 2592000, 2678400
    },
};
#define SEC_PER_DY              (86400)
#define SEC_PER_HR              (3600)


/**
 * Returns 1 if current year id a leap year
 */
static inline int  is_leap(int yr)
{
    if (!(yr % 100))
        return (yr % 400 == 0) ? 1 : 0;
    else
        return (yr % 4 == 0) ? 1 : 0;
}

static unsigned char  day_of_week_get(unsigned char month, unsigned char day, unsigned short year)
{
    /* Month should be a number 0 to 11, Day should be a number 1 to 31 */

    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    year -= month < 3;
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}


time_t  mktime(struct tm *tm)
{
    int i;
    int leapyr = 0;

    time_t ret = 0;

    for (i = 1970; i < (tm->tm_year + 1900); i++)
        ret += SEC_PER_YR[is_leap(i)];

    if (is_leap(tm->tm_year + 1900))
        leapyr = 1;

    for (i = 0; i < (tm->tm_mon); i++) {
        ret += SEC_PER_MT[leapyr][i];
    }

    ret += ((tm->tm_mday) - 1) * SEC_PER_DY;
    ret += (tm->tm_hour) * SEC_PER_HR;
    ret += (tm->tm_min) * 60;
    ret += tm->tm_sec;

    return ret;
}

struct tm   *gmtime_r(const time_t *time, struct tm *result)
{
    int leapyr = 0;
    time_t ltime = *time;

    memset(result, 0, sizeof(struct tm));
    result->tm_year = 1970;

    while (1) {
        if (ltime < SEC_PER_YR[is_leap(result->tm_year)]) {
            break;
        }
        ltime -= SEC_PER_YR[is_leap(result->tm_year)];
        result->tm_year++;
    }

    leapyr = is_leap(result->tm_year);

    while (1) {
        if (ltime < SEC_PER_MT[leapyr][result->tm_mon])
            break;
        ltime -= SEC_PER_MT[leapyr][result->tm_mon];
        result->tm_mon++;
    }

    result->tm_mday = ltime / SEC_PER_DY;
    result->tm_mday++;
    ltime = ltime % SEC_PER_DY;

    result->tm_hour = ltime / SEC_PER_HR;
    ltime = ltime % SEC_PER_HR;

    result->tm_min = ltime / 60;
    result->tm_sec = ltime % 60;

    result->tm_wday = day_of_week_get(result->tm_mon + 1, result->tm_mday, result->tm_year);

    /*
     * Solve bug WMSDK-27. 'man gmtime' says:
     * "tm_year   The number of years since 1900."
     */
    result->tm_year -= 1900;

    return result;
}


