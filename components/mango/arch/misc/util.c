/*
 * util.c
 *
 *  Created on: 2015-7-24
 *      Author: kanjie
 */
#include "util.h"
#include  <string.h>
#include  <stdint.h>
#include  <stdio.h>

#define ZEROPAD     (1 << 0)    /* pad with zero */
#define SIGN        (1 << 1)    /* unsigned/signed long */
#define PLUS        (1 << 2)    /* show plus */
#define SPACE       (1 << 3)    /* space if plus */
#define LEFT        (1 << 4)    /* left justified */
#define SPECIAL     (1 << 5)    /* 0x */
#define LARGE       (1 << 6)    /* use 'ABCDEF' instead of 'abcdef' */


//判断内存是否全部填充某个字符
//返回长度 和 样本长度相同 表示匹配
//小于样本长度 表示发现不匹配的首位置
size_t  memchcmp(const void *s, uint8_t c, size_t n)
{
    size_t u = 0;
    uint8_t *p = (uint8_t *)s;

    for (u = 0; u < n; u++)
        if (c != p[u])break;

    return u;
}

/*
 * Like memcmp, but case-insensitive and always returns -1 if different; from mbedtls\x509_crt.c
 */
int memcasecmp(const void *s1, const void *s2, size_t len)
{
    size_t i;
    unsigned char diff;
    const unsigned char *n1 = s1, *n2 = s2;

    for (i = 0; i < len; i++) {
        diff = n1[i] ^ n2[i];

        if (diff == 0)
            continue;

        if (diff == 32 &&
                ((n1[i] >= 'a' && n1[i] <= 'z') ||
                 (n1[i] >= 'A' && n1[i] <= 'Z'))) {
            continue;
        }

        return (-1);
    }

    return (0);
}

char *strrstr(const char *s1, const char *s2)
{
    if (s1 && *s1 && s2 && *s2) {
        char *s1_r = (char *)(s1 + strlen(s1) - 1);
        size_t s2_len = strlen(s2);
        while (s1_r >= s1) {
            if (0 == strncmp(s1_r, s2, s2_len))
                return s1_r;
            s1_r--;
        }
    }

    return NULL;
}

char *strnchr(const char *s1, char c, size_t n)
{
    if (NULL == s1 || 0 == n || '\0' == c)
        return NULL;

    for (size_t i = 0; i < n; i++) {
        if (s1[i] == '\0')
            break;
        if (s1[i] == c)
            return (char *)&s1[i];
    }
    return NULL;
}

int str_cut_head_c(char *str, int str_len, char c)
{
    int c_nums = 0;

    if (str_len == 0)
        str_len = strlen(str);

    for (int i = 0; i < str_len; i++) {
        if (str[i] != c)
            break;
        c_nums++;
    }

    if (c_nums) {
        for (int i = 0; i < str_len; i++) {
            if (i < str_len - c_nums)
                str[i] = str[i + c_nums];
            else
                str[i] = '\0';
        }
    }

    return c_nums;
}

int str_cut_tail_c(char *str, int str_len, char c)
{
    int c_nums = 0;

    if (str_len == 0)
        str_len = strlen(str);

    for (int i = str_len - 1; i >= 0; i--) {
        if (str[i] != c)
            break;
        str[i] = '\0';
        c_nums++;
    }

    return c_nums;
}

//转换为无符号数
//n表示读取数字字符的最大长度
uint32_t  arch_atoun(const char *c, size_t n)
{
    uint32_t dig = 0;
    const char *org = c;
    while (isdigit((int)*c) && (c - org < n)) {
        dig = dig * 10 + *c - '0';
        c++;
    }
    return dig;
}

double arch_atofn(const char *c, size_t n)
{
    double val = 0.0;
    const char *c_end;

    if (c == NULL || n == 0)goto finish_exit;

    c_end = c + n;

    while (c < c_end && isspace((int)*c))c++;

    if (c >= c_end)goto finish_exit;

    {
        int flag = 1;
        if (*c == '-')flag = -1;
        if (*c == '+' || *c == '-')c++;

        while (c < c_end && isdigit((int)*c)) {
            val = val * 10.0 + (*c - '0');
            c++;
        }

        double power = 1.0;
        if (c < c_end && *c == '.') {
            c++;
            while (c < c_end && isdigit((int)*c)) {
                val = val * 10.0 + (*c - '0');
                power *= 10.0;
                c++;
            }
        }
        val = (flag * val) / power;
    }

    if (c >= c_end)goto finish_exit;

    if (*c == 'e' || *c == 'E') {
        int flag = 1;
        int e = 0;
        if (++c < c_end) {
            if (*c == '-')flag = -1;
            if (*c == '+' || *c == '-')c++;
        }

        while (c < c_end && isdigit((int)*c)) {
            e = e * 10 + (*c - '0');
            c++;
        }

        if (flag == -1) {
            while (e-- > 0)val /= 10.0;
        } else {
            while (e-- > 0)val *= 10.0;
        }
    }

finish_exit:

    return val;
}

//16进制数串转换为无符号数
uint32_t  arch_axtou(const char *c)
{
    uint32_t dig = 0;
//  const char *org = c;
    while (isxdigit((int)*c)) {
        dig <<= 4;
        dig += 0x0000000F & hex_char_value((int) * c);
        c++;
    }
    return dig;
}

//16进制数串转换为无符号数 n限制采样字符数
uint32_t  arch_axtoun(const char *c, size_t n)
{
    int dig = 0;
    const char *org = c;
    while (isxdigit((int)*c) && (c - org < n)) {
        dig = dig * 16 + hex_char_value((int) * c);
        c++;
    }
    return dig;
}

//将HEX字符串转换为二进制数组
//in尺寸有限制
//out空间有限制
//in_len输出已处理输入长度
//返回输出长度
//arch_axtobuf("12EF985xyz",out) 返回6，最后一个'5'被抛弃
size_t  arch_axtobuf(const char *in, size_t in_size, uint8_t *out, size_t out_size, size_t *in_len)
{
    const char *org_in = in;
    uint8_t *org_out = out;

    while (isxdigit((int)*in) && isxdigit((int) * (in + 1)) && //输入不硌牙
            (in  - org_in <  in_size) && (out - org_out < out_size)) {  // 不超限

        *out = (0x0F & hex_char_value((int) * (in + 1))) | (0xF0 & (hex_char_value((int) * in) << 4)); //转换处理

        in += 2; out += 1;  //调整指针
    }

    if (in_len)
        *in_len = in  - org_in;

    return out - org_out;
}
//转换为无符号的数，n表示读取数字字符的最大长度。
uint64_t arch_atou64n(const char *c, size_t n)
{
    uint64_t dig = 0;
    const char *org = c;
    while (isdigit((int)*c) && (c - org < n)) {
        dig = dig * 10 + *c - '0';
        c++;
    }
    return dig;
}

int  snprintf_hex(char *buf, size_t buf_size, const uint8_t *data, size_t len, char style)
{
    unsigned char spliter, uppercase;
    size_t n = 0;

    if (buf_size == 0)
        return 0;

    spliter = style & 0x7F;
    uppercase = (style & 0x80) ? 1 : 0;

    if (!isprint(spliter))
        spliter = 0;

    for (size_t i = 0; i < len; i++) {
        //若需要打印分隔符
        if (i < len - 1 && spliter)
            n += snprintf(buf + n, buf_size - n, (uppercase ? "%02X%c" : "%02x%c"), data[i], spliter);
        else
            n += snprintf(buf + n, buf_size - n, (uppercase ? "%02X" : "%02x"), data[i]);
        if (n >= buf_size) break;
    }

    return (n >= buf_size) ? (buf_size - 1) : n;
}

//转换为无符号数
//n表示读取数字字符的最大长度
int64_t arch_atos64n(const char *c, size_t n)
{
    if (*c == '-')
        return 0 - (int64_t) arch_atou64n(c + 1, n - 1);
    else
        return (int64_t) arch_atou64n(c, n);
}


//转换为无符号数
//n表示读取数字字符的最大长度
int32_t arch_atoin(const char *c, size_t n)
{
    if (*c == '-')
        return 0 - (int32_t) arch_atoun(c + 1, n - 1);
    else
        return (int32_t) arch_atoun(c, n);
}


int arch_u64toa(uint64_t data, char *c)
{
    int num = 0, index;
    char tmp;

    while (data > 0) {
        c[num++] = data % 10 + '0';
        data /= 10;
    }
    c[num] = '\0';
    for (index = 0; index < (num >> 1); index++) {
        tmp = c[index];
        c[index] = c[num - 1 - index];
        c[num - 1 - index] = tmp;
    }

    return num;
}

int arch_s64toa(int64_t data, char *c)
{
    if (data < 0) {
        c[0] = '-';
        data = -data;
        return (1 + arch_u64toa(data, c + 1));
    }

    return arch_u64toa(data, c);
}

bool str_all_c(uint8_t *str, uint8_t c, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (str[i] != c)
            return false;
    }
    return true;
}

void arch_str2hex(uint8_t *hex, const char *str, int hex_len)
{
    unsigned int tmp;
    int i;
    for (i = 0; i < hex_len; i++) {
        sscanf(&str[i * 2], "%02x", &tmp);
        hex[i] = tmp;
    }
}

void arch_hex2str(char *str, const uint8_t *hex, int hex_len)
{
    int i;
    for (i = 0; i < hex_len; i++)
        sprintf(&str[i * 2], "%02x", hex[i]);
}
