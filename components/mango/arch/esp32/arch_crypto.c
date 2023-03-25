#include "arch_crypto.h"
#include "arch_dbg.h"
#if ETSS_KEY_EXCHANGE_ENABLE
#include "mbedtls/ccm.h"

#define CCM_TAG_SIZE 8
#endif

size_t PKCS7_padding(uint8_t *buf, size_t len, uint8_t base)
{
    uint8_t padding_len;

    //取填充字节数（若对齐，则取base）
    padding_len = base - len % base;
    if (0 == padding_len)padding_len = base;

    //if buf is null, only return padding len;
    if (buf) {
        for (int i = 0; i < padding_len; i++)
            buf[len + i] = padding_len;
    }

    return padding_len;
}

size_t PKCS7_depadding(uint8_t *buf, size_t len, uint8_t base)
{
    //剔除padding
    uint8_t *ptr_pad = buf + len - 1;
    uint8_t pad = *ptr_pad;
    if (pad > base || len < base) {
        return 0;
    }

    uint8_t padding_len = pad;

    for (; pad > 0; pad--)
        *ptr_pad-- = '\0';

    return padding_len;
}

void arch_aes_encrypt_cbc(uint8_t *pPlainTxt, size_t plainTxtLen, uint8_t *pCipTxt, size_t *cipTxtLen, uint8_t *pKey, size_t keyBytes, uint8_t *iv)
{
    esp_aes_context aes_ctx;

    esp_aes_setkey(&aes_ctx, pKey, keyBytes * 8);

    esp_aes_crypt_cbc(&aes_ctx, ESP_AES_ENCRYPT, plainTxtLen, iv, (const uint8_t *)pPlainTxt, pCipTxt);

    *cipTxtLen = plainTxtLen;
}


void arch_aes_decrypt_cbc(uint8_t *pCipTxt, size_t cipTxtLen, uint8_t *pPlainTxt, size_t *plainTxtlen, uint8_t *pKey, size_t keyBytes, uint8_t *iv)
{
    esp_aes_context aes_ctx;

    esp_aes_setkey(&aes_ctx, pKey, keyBytes * 8);

    esp_aes_crypt_cbc(&aes_ctx, ESP_AES_DECRYPT, cipTxtLen, iv, (const uint8_t *)pCipTxt, pPlainTxt);

    *plainTxtlen = cipTxtLen;
}

void arch_md5(uint8_t *input, int len, uint8_t hash[16])
{
    struct MD5Context md5;

    MD5Init(&md5);
    MD5Update(&md5, input, len);
    MD5Final(hash, &md5);
}
#if ETSS_KEY_EXCHANGE_ENABLE
/**
 * @brief AES-128-CCM encryption
 *
 * @param N : nonce
 * @param n_len : length of nonce
 * @param A : addtion data
 * @param a_len
 * @param P : plain
 * @param p_len
 * @param K : key
 * @param k_len
 * @param C : cipher
 * @param c_len
 *
 * @return 0/errno
 */
int arch_aes_ccm_enc(uint8_t n_len, const uint8_t *N,
                     uint8_t a_len,  const uint8_t *A,
                     uint8_t p_len,  const uint8_t *P,
                     uint8_t k_len,  const uint8_t *K,
                     uint8_t *c_len, uint8_t *C
                    )
{
    int ret = 0;
    mbedtls_ccm_context ctx;

    mbedtls_ccm_init(&ctx);

    if ((ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, K, 8 * k_len)))
        goto done;

    ret = mbedtls_ccm_encrypt_and_tag(&ctx, p_len,
                                      N, n_len, A, a_len,
                                      P, C,
                                      C + p_len, CCM_TAG_SIZE);
    *c_len = p_len + CCM_TAG_SIZE;

done:
    mbedtls_ccm_free(&ctx);
    return ret;
}

/**
 * @brief AES-128-CCM decrption
 *
 * @param N : nonce
 * @param n_len : length of nonce
 * @param A : addtion data
 * @param a_len
 * @param C : cipher
 * @param c_len
 * @param K : key
 * @param k_len
 * @param P : plain
 * @param p_len
 *
 * @return 0/errno
 */
int arch_aes_ccm_dec(uint8_t n_len, const uint8_t *N,
                     uint8_t a_len, const uint8_t *A,
                     uint8_t c_len, const uint8_t *C,
                     uint8_t k_len, const uint8_t *K,
                     uint8_t *p_len, uint8_t *P
                    )
{
    int ret = 0;
    mbedtls_ccm_context ctx;

    mbedtls_ccm_init(&ctx);

    if ((ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, K, 8 * k_len)))
        goto done;

    *p_len = c_len - CCM_TAG_SIZE;
    ret = mbedtls_ccm_auth_decrypt(&ctx, *p_len,
                                   N, n_len, A, a_len,
                                   C, P,
                                   C + *p_len, CCM_TAG_SIZE);

done:
    mbedtls_ccm_free(&ctx);
    return ret;
}
#endif
