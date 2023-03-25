#ifndef _ARCH_CRYPTO_H_
#define _ARCH_CRYPTO_H_

#include "arch_chip.h"

size_t PKCS7_padding(uint8_t *buf, size_t len, uint8_t base);
size_t PKCS7_depadding(uint8_t *buf, size_t len, uint8_t base);
void arch_aes_encrypt_cbc(uint8_t *pPlainTxt, size_t textLen, uint8_t *pCipTxt, size_t *cipTxtLen, uint8_t *pKey, size_t keyBytes, uint8_t *iv);
void arch_aes_decrypt_cbc(uint8_t *pCipTxt, size_t textLen, uint8_t *pPlainTxt, size_t *plainTxtlen, uint8_t *pKey, size_t keyBytes, uint8_t *iv);
void arch_md5(uint8_t *input, int len, uint8_t hash[16]);

#if ETSS_KEY_EXCHANGE_ENABLE
int arch_aes_ccm_enc(uint8_t n_len, const uint8_t *N,
                     uint8_t a_len,  const uint8_t *A,
                     uint8_t p_len,  const uint8_t *P,
                     uint8_t k_len,  const uint8_t *K,
                     uint8_t *c_len, uint8_t *C
                    );

int arch_aes_ccm_dec(uint8_t n_len, const uint8_t *N,
                     uint8_t a_len, const uint8_t *A,
                     uint8_t c_len, const uint8_t *C,
                     uint8_t k_len, const uint8_t *K,
                     uint8_t *p_len, uint8_t *P
                    );
#endif
#endif
