#ifndef __ARCH_PSM_YEE_H__
#define __ARCH_PSM_YEE_H__


#include "arch_flash.h"

#define wmprintf ets_printf

typedef void *psm_hnd_t;
typedef void *psm_object_handle_t;


#define MOD_ERROR_START(x)  (x << 12 | 0)
#define MOD_WARN_START(x)   (x << 12 | 1)
#define MOD_INFO_START(x)   (x << 12 | 2)

/* Create Module index */
#define MOD_GENERIC    0

/* Globally unique success code */
#define WM_SUCCESS 0

/* enable secure by xiaomi */
//#define CONFIG_SECURE_PSM 1

enum wm_errno {
    /* First Generic Error codes */
    WM_GEN_E_BASE = MOD_ERROR_START(MOD_GENERIC),
    WM_FAIL,
    WM_E_PERM,   /* Operation not permitted */
    WM_E_NOENT,  /* No such file or directory */
    WM_E_SRCH,   /* No such process */
    WM_E_INTR,   /* Interrupted system call */
    WM_E_IO,     /* I/O error */
    WM_E_NXIO,   /* No such device or address */
    WM_E_2BIG,   /* Argument list too long */
    WM_E_NOEXEC, /* Exec format error */
    WM_E_BADF,   /* Bad file number */
    WM_E_CHILD,  /* No child processes */
    WM_E_AGAIN,  /* Try again */
    WM_E_NOMEM,  /* Out of memory */
    WM_E_ACCES,  /* Permission denied */
    WM_E_FAULT,  /* Bad address */
    WM_E_NOTBLK, /* Block device required */
    WM_E_BUSY,   /* Device or resource busy */
    WM_E_EXIST,  /* File exists */
    WM_E_XDEV,   /* Cross-device link */
    WM_E_NODEV,  /* No such device */
    WM_E_NOTDIR, /* Not a directory */
    WM_E_ISDIR,  /* Is a directory */
    WM_E_INVAL,  /* Invalid argument */
    WM_E_NFILE,  /* File table overflow */
    WM_E_MFILE,  /* Too many open files */
    WM_E_NOTTY,  /* Not a typewriter */
    WM_E_TXTBSY, /* Text file busy */
    WM_E_FBIG,   /* File too large */
    WM_E_NOSPC,  /* No space left on device */
    WM_E_SPIPE,  /* Illegal seek */
    WM_E_ROFS,   /* Read-only file system */
    WM_E_MLINK,  /* Too many links */
    WM_E_PIPE,   /* Broken pipe */
    WM_E_DOM,    /* Math argument out of domain of func */
    WM_E_RANGE,  /* Math result not representable */
    WM_E_CRC,    /* Error in CRC check */
};

#if 0
//this is only for psm
typedef signed char             int8_t;
typedef short int               int16_t;
typedef int                     int32_t;
typedef long long int           int64_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long long int  uint64_t;
typedef signed char             byte;
#endif


/*
 * Note that though this structure is open for all to see, it should not be
 * edited by the caller. It is solely for the use of the AES driver. As
 * shown in above example, it needs to be allocated by the caller.
 */
typedef struct {
    /*
     * The following parameters will be cached in the call to *setkey*.
     * Ensure that this remains in sync with AES_Config_Type. Only
     * required members are cached. Add if necessary in future.
     */
    int mode;
    /* of the type AES_EncDecSel_Type. Use directly */
    int dir;
    /* Already processed as required by h/w */
    uint32_t initVect[4];
    uint32_t keySize;
    /* Already processed as required by h/w */
    uint32_t saved_key[8];
    int micLen;
    int micEn;

    /* For ctr mode */
    uint32_t ctr_mod; /* counter modular */

    /* for CCM mode */
    uint32_t aStrLen; /* associated auth. data len */
} aes_t;

#if 1//defined (__ICCARM__)   /* IAR Compiler */
#define PACK_START
#define PACK_END
//#define WEAK  #pragma weak// also ok
#elif defined(__GNUC__)     /* GNU GCC Compiler */
#define PACK_START __packed
#define PACK_END
#endif


//"os_mem_alloc"... is only for psm
#if 1 //defined (__ICCARM__)   /* IAR Compiler */
#define mdev_t void
#define os_mem_alloc malloc
#define os_mem_realloc realloc
#define os_mem_free free
#define os_mem_calloc(size) calloc(1,size)
#else
#if defined(BUILD_MINGW) && defined(__PSM_UTIL__)
size_t strnlen(const char *s, size_t maxlen);
#endif

#if defined(__linux__) || defined(__PSM_UTIL__)
#define mdev_t void

#define os_mem_alloc malloc
#define os_mem_realloc realloc
#define os_mem_free free
static void *os_mem_calloc(int size)
{
    void *ptr = malloc(size);
    if (!ptr)
        return NULL;

    memset(ptr, 0x00, size);
    return ptr;
}
#endif /* __linux__ */
#endif


//the api is only for psm
int flash_drv_erase_sector(uint32_t start, uint32_t size);


#define os_rwlock_create(plock,m_name,l_name) arch_os_rwlock_create(plock)
#define os_rwlock_create_with_cb(plock,m_name,l_name,fn) arch_os_rwlock_create_with_cb(plock,fn)
#define os_rwlock_read_lock arch_os_rwlock_read_lock
#define os_rwlock_read_unlock arch_os_rwlock_read_unlock
#define os_rwlock_write_lock arch_os_rwlock_write_lock
#define os_rwlock_write_unlock arch_os_rwlock_write_unlock
#define os_rwlock_delete arch_os_rw_lock_delete

#define os_semaphore_create(handle,name)  arch_os_semaphore_create(handle)
#define os_semaphore_create_counting(handle,name,maxcnt,initcnt)  arch_os_semaphore_create_counting(handle,maxcnt,initcnt)
#define os_semaphore_get    arch_os_semaphore_get
#define os_semaphore_put    arch_os_semaphore_put
#define os_semaphore_getcount   arch_os_semaphore_getcount
#define os_semaphore_delete arch_os_semaphore_delete

#define get_random_sequence arch_os_get_random
#define wmtime_time_get_posix arch_os_ms_now//注意，此处仅仅用于psm去作为srand的种子，不要用在其他地方
#define pm_mcu_state(a,b)   //this is for psm-test-main.c


typedef struct flash_desc {
    /** The flash device */
    uint8_t   fl_dev;
    /** The start address on flash */
    uint32_t  fl_start;
    /** The size on flash  */
    uint32_t  fl_size;
} flash_desc_t;


//flash_drv_xxx is only add for psm
//psm在competion的时候，会重新擦出flash，这里应该擦除psm除去swap的所有部分
//#define flash_drv_erase(mdev,start,size) spi_flash_erase_sector(start>>12)     // 以4K为单位
#define flash_drv_erase(mdev,start,size) flash_drv_erase_sector(start,size)
#define flash_drv_read(mdev, buf,len, addr)  arch_flash_read(addr, buf, len)
#define flash_drv_write(mdev, buf,len, addr)  arch_psm_flash_write(addr, buf, len)
#define flash_drv_close(mdev)     arch_psm_flash_close(mdev)
#define flash_drv_open(fl_dev)    arch_psm_flash_open
#define flash_drv_init()



#define os_timer_create arch_os_timer_create
#define os_timer_activate   arch_os_timer_activate
#define os_timer_change arch_os_timer_change
#define os_timer_is_active  arch_os_timer_is_active
#define os_timer_get_context    arch_os_timer_get_context
#define os_timer_reset  arch_os_timer_reset
#define os_timer_deactivate arch_os_timer_deactivate
#define os_timer_delete arch_os_timer_delete

psm_hnd_t psm_get_handle(void);
void psm_set_handle(psm_hnd_t hnd);
int arch_psm_get_desc(flash_desc_t *f);




#endif //__ARCH_PSM_YEE_H__
