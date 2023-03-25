#ifndef __ARCH_OS_YEE_H__
#define __ARCH_OS_YEE_H__

#include "arch_chip.h"

#define OS_WAIT_FOREVER (0xFFFFFFFF)

typedef void *os_mbox_pt;
typedef void os_semaphore_t;
typedef void os_mutex_t;

typedef struct _rw_lock os_rw_lock_t;
typedef int (*cb_fn)(struct _rw_lock *plock, unsigned int wait_time);
typedef struct _rw_lock {
    /** Mutex for reader mutual exclusion */
    os_mutex_t *reader_mutex;
    /** Lock which when held by reader, writer cannot enter critical section */
    os_semaphore_t *rw_lock;
    /** Function being called when first reader gets the lock */
    cb_fn reader_cb;
    /** Counter to maintain number of readers in critical section */
    unsigned int reader_count;
} os_rw_lock_t;
/** This is prototype of reader callback */

int arch_os_rwlock_create_with_cb(os_rw_lock_t *plock, cb_fn r_fn);
int arch_os_rwlock_create(os_rw_lock_t *plock);
void arch_os_rw_lock_delete(os_rw_lock_t *lock);
int arch_os_rwlock_write_lock(os_rw_lock_t *lock, unsigned int wait_time);
int arch_os_rwlock_read_lock(os_rw_lock_t *lock, unsigned int wait_time);
int arch_os_rwlock_read_unlock(os_rw_lock_t *lock);
void arch_os_rwlock_write_unlock(os_rw_lock_t *lock);

#endif//__ARCH_OS_YEE_H__
