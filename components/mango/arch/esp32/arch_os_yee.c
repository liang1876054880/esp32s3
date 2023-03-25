#include "arch_os.h"
#include "arch_dbg.h"
#include "arch_os_yee.h"

/******************OS-RWLOCK******************/

int arch_os_rwlock_create(os_rw_lock_t *plock)
{
    return arch_os_rwlock_create_with_cb(plock, NULL);
}

int arch_os_rwlock_create_with_cb(os_rw_lock_t *plock, cb_fn r_fn)
{
    int ret = ETSS_OK;
    ret = arch_os_mutex_create(&(plock->reader_mutex));
    if (ret == ETSS_ERROR) {
        return ETSS_ERROR;
    }
    ret = arch_os_semaphore_create(&(plock->rw_lock));
    if (ret == ETSS_ERROR) {
        return ETSS_ERROR;
    }
    plock->reader_count = 0;
    plock->reader_cb = r_fn;
    return ret;
}

int arch_os_rwlock_read_lock(os_rw_lock_t *lock,
                             unsigned int wait_time)
{
    int ret = ETSS_OK;
    ret = arch_os_mutex_get((lock->reader_mutex), OS_WAIT_FOREVER);
    if (ret == ETSS_ERROR) {
        return ret;
    }
    lock->reader_count++;
    if (lock->reader_count == 1) {
        if (lock->reader_cb) {
            ret = lock->reader_cb(lock, wait_time);
            if (ret == ETSS_ERROR) {
                lock->reader_count--;
                arch_os_mutex_put((lock->reader_mutex));
                return ret;
            }
        } else {
            /* If  1 it is the first reader and
             * if writer is not active, reader will get access
             * else reader will block.
             */
            ret = arch_os_semaphore_get((lock->rw_lock),
                                        wait_time);
            if (ret == ETSS_ERROR) {
                lock->reader_count--;
                arch_os_mutex_put((lock->reader_mutex));
                return ret;
            }
        }
    }
    arch_os_mutex_put((lock->reader_mutex));
    return ret;
}

int arch_os_rwlock_read_unlock(os_rw_lock_t *lock)
{
    int ret = arch_os_mutex_get((lock->reader_mutex), OS_WAIT_FOREVER);

    if (ret == ETSS_ERROR) {
        return ret;
    }
    lock->reader_count--;
    if (lock->reader_count == 0) {
        /* This is last reader so
         * give a chance to writer now
         */
        arch_os_semaphore_put((lock->rw_lock));
    }

    arch_os_mutex_put((lock->reader_mutex));
    return ret;
}

int arch_os_rwlock_write_lock(os_rw_lock_t *lock,
                              unsigned int wait_time)
{

    int ret = arch_os_semaphore_get((lock->rw_lock),
                                    wait_time);
    return ret;
}


void arch_os_rwlock_write_unlock(os_rw_lock_t *lock)
{
    arch_os_semaphore_put((lock->rw_lock));
}


void arch_os_rw_lock_delete(os_rw_lock_t *lock)
{
    lock->reader_cb = NULL;
    arch_os_semaphore_delete((lock->rw_lock));
    arch_os_mutex_delete((lock->reader_mutex));
    lock->reader_count = 0;
}
