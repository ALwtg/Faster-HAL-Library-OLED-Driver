#ifndef __OLED_H
#define __OLED_H

#include <stdint.h> 

/* 基础控制函数 */
void OLED_Init(void);
void OLED_Clear(void);

/* 显存刷新函数*/
void OLED_Update(void); // 将显存缓冲区全部写入屏幕
void OLED_Refresh_Area(uint8_t x, uint8_t y, uint8_t width, uint8_t height); // 刷新指定区域

/* 基础显示函数 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* 绘图与高级显示函数 */
void OLED_ShowBMP(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bmp, uint8_t mode);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t mode); // 此函数只写内存，不直接显示
void OLED_ShowString_Size(uint8_t Line, uint8_t Column, char *String, uint8_t Size);

#endif
