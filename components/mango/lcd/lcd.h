#ifndef __LCD_H
#define __LCD_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#define USE_HORIZONTAL 2  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏

#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
#define LCD_W 135
#define LCD_H 240
#else
#define LCD_W 240
#define LCD_H 135
#endif


// 画笔颜色
#define WHITE            0xFFFF
#define BLACK            0x0000
#define BLUE             0x001F
#define BRED             0XF81F
#define GRED             0XFFE0
#define GBLUE            0X07FF
#define RED              0xF800
#define MAGENTA          0xF81F
#define GREEN            0x07E0
#define CYAN             0x7FFF
#define YELLOW           0xFFE0
#define BROWN            0XBC40 //棕色
#define BRRED            0XFC07 //棕红色
#define GRAY             0X8430 //灰色
#define DARKBLUE         0X01CF //深蓝色
#define LIGHTBLUE        0X7D7C //浅蓝色
#define GRAYBLUE         0X5458 //灰蓝色
#define LIGHTGREEN       0X841F //浅绿色
#define LGRAY            0XC618 //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

// #define PIN_NUM_MISO   5 // SPI MISO
#define PIN_NUM_MOSI  18 // SPI MOSI
#define PIN_NUM_CLK   23 // SPI CLOCK pin
#define PIN_NUM_CS    16 // Display CS pin
#define PIN_NUM_DC    4  // Display command/data pin

#define PIN_NUM_RST   17 // GPIO used for RESET control
#define PIN_NUM_BCKL  19 // GPIO used for backlight control

//-----------------LCD端口定义----------------
//SCL=SCLK
#define LCD_SCLK_Clr()     gpio_set_level(PIN_NUM_CLK, 0)
#define LCD_SCLK_Set()     gpio_set_level(PIN_NUM_CLK, 1)

//SDA=MOSI
#define LCD_MOSI_Clr()     gpio_set_level(PIN_NUM_MOSI, 0)
#define LCD_MOSI_Set()     gpio_set_level(PIN_NUM_MOSI, 1)

//RES
#define LCD_RES_Clr()      gpio_set_level(PIN_NUM_RST, 0)
#define LCD_RES_Set()      gpio_set_level(PIN_NUM_RST, 1)

//DC
#define LCD_DC_Clr()       gpio_set_level(PIN_NUM_DC, 0)
#define LCD_DC_Set()       gpio_set_level(PIN_NUM_DC, 1)

//CS
#define LCD_CS_Clr()       gpio_set_level(PIN_NUM_CS, 0)
#define LCD_CS_Set()       gpio_set_level(PIN_NUM_CS, 1)

//BLK
#define LCD_BLK_Clr()      gpio_set_level(PIN_NUM_BCKL, 0)
#define LCD_BLK_Set()      gpio_set_level(PIN_NUM_BCKL, 1)


void LCD_GPIO_Init(void);       // 初始化GPIO
void LCD_Init(void);            // LCD初始化
void LCD_Writ_Bus(uint8_t dat); // 模拟SPI时序
void LCD_WR_DATA8(uint8_t dat); // 写入一个字节
void LCD_WR_DATA(uint16_t dat); // 写入两个字节
void LCD_WR_REG(uint8_t dat);   // 写入一个指令

void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);                  // 设置坐标函数
void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color); // 指定区域填充颜色
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);                                // 在指定位置画一个点
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);     // 在指定位置画一条线
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color); // 在指定位置画一个矩形
void Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);                     // 在指定位置画一个圆

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);  // 显示一个字符
void LCD_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // 显示字符串
uint32_t mypow(uint8_t m, uint8_t n);                                                                           // 求幂
void LCD_ShowIntNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey); // 显示整数变量
void LCD_ShowFloatNum1(uint16_t x, uint16_t y, float num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey); // 显示两位小数变量
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t pic[]);             // 显示图片


void showhanzi16(uint16_t x, uint16_t y, uint8_t  index, uint16_t point_color, uint16_t back_color);
#endif





