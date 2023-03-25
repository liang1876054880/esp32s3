#ifndef __ARCH_TIME_H__
#define __ARCH_TIME_H__

#include "arch_chip.h"

/**
 * Convert to tm structure from POSIX/Unix time (Seconds since epoch)
 *
 * \param[in] time This is POSIX time that is to be converted into \ref tm
 * \param[out] result This should point to pre-allocated \ref tm instance
 * \return pointer to struct tm; NULL in case of error
 */
struct tm *gmtime_r(const time_t *time, struct tm *result);

/**
 * Converts to POSIX/Unix time from tm structure
 *
 * \param[in] tm This is \ref tm instance that is to be converted into
 * time_t format
 * \return time_t POSIX/Unix time equivalent
 */
time_t mktime(struct tm *tm);

#endif  /* __ARCH_TIME_H__ */
