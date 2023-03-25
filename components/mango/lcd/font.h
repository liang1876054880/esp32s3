#ifndef __FONT_H
#define __FONT_H

//QQ图像
// const unsigned char qqimage[3200];

typedef struct {
    unsigned char Index[2];
    unsigned char Msk[24];
} typFNT_GB12;

const unsigned char ascii_1206[95][12];
const unsigned char ascii_1608[95][16];
const unsigned char ascii_2412[95][48];
const unsigned char ascii_3216[95][64];
const unsigned char gImage_1[3200];
const unsigned char hanzi16[];


#endif
