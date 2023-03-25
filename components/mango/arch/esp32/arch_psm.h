#ifndef __ARCH_PSM_H__
#define __ARCH_PSM_H__

#include "arch_chip.h"

int arch_psm_init(void);
int arch_psm_erase_key(const char *name_space, const char *key);
int arch_psm_erase_country_domain(const char *key);
int arch_psm_get_value(const char *name_space, const char *key, void *value, size_t length);
int arch_psm_set_value(const char *name_space, const char *key, const void *value, size_t length);
int arch_psm_get_str(const char *name_space, const char *key, char *str, size_t str_size);
int arch_psm_get_country_domain(const char *key, char *str, size_t str_size);
int arch_psm_set_str(const char *name_space, const char *key, const char *str);
int arch_psm_set_country_domain(const char *key, char *str);

#endif /* __ARCH_PSM_H__ */

