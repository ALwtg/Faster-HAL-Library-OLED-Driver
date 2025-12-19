#include "stm32f1xx_hal.h"
#include "OLED_Font.h"

/*引脚配置*/
#define OLED_W_SCL(x)		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, (GPIO_PinState)(x))
#define OLED_W_SDA(x)		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, (GPIO_PinState)(x))

// 显存缓冲区 [8页][128列]
static uint8_t OLED_Buffer[8][128]; 

/*引脚初始化*/
void OLED_I2C_Init(void)
{
	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD; // 开漏输出
	GPIO_InitStructure.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C停止
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}

/**
  * @brief  OLED写命令
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  OLED设置光标位置
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}

/**
  * @brief  [优化核心] 局部刷新函数
  * @note   将显存 OLED_Buffer 中的指定区域刷到屏幕上
  * @param  x      起始列 (0~127)
  * @param  y      起始像素行 (0~63)
  * @param  width  刷新宽度
  * @param  height 刷新高度
  */
void OLED_Refresh_Area(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    uint8_t i, n;
    
    // 边界检查
    if (x >= 128 || y >= 64) return;
    if (x + width > 128) width = 128 - x;
    if (y + height > 64) height = 64 - y;

    // 计算涉及的页(Page)范围
    uint8_t page_start = y / 8;
    uint8_t page_end = (y + height - 1) / 8;

    for (i = page_start; i <= page_end; i++)
    {
        OLED_SetCursor(i, x); // 设置光标到该页的起始列
        
        OLED_I2C_Start();
        OLED_I2C_SendByte(0x78);
        OLED_I2C_SendByte(0x40); // 连续写数据模式
        
        for (n = 0; n < width; n++)
        {
            OLED_I2C_SendByte(OLED_Buffer[i][x + n]); // 从缓冲区取数据
        }
        OLED_I2C_Stop();
    }
}

/**
  * @brief  [优化核心] 全屏刷新
  */
void OLED_Update(void)
{
    OLED_Refresh_Area(0, 0, 128, 64);
}

/**
  * @brief  [优化] OLED清屏
  * @note   现在只清空缓冲区并刷新，速度更快
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		for(i = 0; i < 128; i++)
		{
			OLED_Buffer[j][i] = 0x00; // 清空内存
		}
	}
    OLED_Update(); // 刷新全屏
}

/**
  * @brief  [优化] OLED画点函数
  * @note   只修改内存，不操作I2C，极大提升速度
  * @param  mode: 1 点亮，0 熄灭
  */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t mode)
{
    if (x >= 128 || y >= 64) return;
    
    uint8_t page = y / 8;
    uint8_t bit_pos = y % 8;
    
    if (mode) {
        OLED_Buffer[page][x] |= (1 << bit_pos);
    } else {
        OLED_Buffer[page][x] &= ~(1 << bit_pos);
    }
}

/**
  * @brief  [优化] OLED显示字符串（支持缩放）
  * @note   先在内存画点，最后一次性刷新，消除扫描感
  */
void OLED_ShowString_Size(uint8_t Line, uint8_t Column, char *String, uint8_t Size)
{
	uint16_t x_start, y_start;
	uint8_t temp, i, j, k, m;
	uint16_t x, y;
    
    // 记录刷新区域
    uint8_t refresh_x, refresh_y, refresh_w, refresh_h;
    uint8_t char_count = 0;
    
	y_start = (Line - 1) * 16;
	x_start = (Column - 1) * 8;
    
    refresh_x = x_start;
    refresh_y = y_start;
	
	while (*String != '\0')
	{
        // 简单换行保护
		if (x_start + 8 * Size > 128) break;
		
		uint8_t c = *String - ' ';
		
		for (i = 0; i < 8; i++)
		{
            // 上半字模
			temp = OLED_F8x16[c][i]; 
			for (j = 0; j < 8; j++)
			{
				uint8_t color = (temp & (1 << j)) ? 1 : 0;
				for (k = 0; k < Size; k++) {     
					for (m = 0; m < Size; m++) { 
						x = x_start + i * Size + k;
						y = y_start + j * Size + m;
						OLED_DrawPoint(x, y, color);
					}
				}
			}
			
            // 下半字模
			temp = OLED_F8x16[c][i + 8]; 
			for (j = 0; j < 8; j++)
			{
				uint8_t color = (temp & (1 << j)) ? 1 : 0;
				for (k = 0; k < Size; k++) {
					for (m = 0; m < Size; m++) {
						x = x_start + i * Size + k;
						y = y_start + (j + 8) * Size + m; 
						OLED_DrawPoint(x, y, color);
					}
				}
			}
		}
		
		x_start += 8 * Size;
		String++;
        char_count++;
	}
    
    // 计算需要刷新的总宽高
    refresh_w = char_count * 8 * Size;
    refresh_h = 16 * Size;
    
    // 一次性刷新涉及的区域
    OLED_Refresh_Area(refresh_x, refresh_y, refresh_w, refresh_h);
}

/* 
   以下函数保留，为了兼容性。
   注意：直接调用 OLED_WriteData 的函数会直接写屏，
   虽然不会利用 Buffer 加速，但可以正常工作。
*/

//void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
//{      	
//	uint8_t i;
//	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
//	for (i = 0; i < 8; i++)
//	{
//        uint8_t data = OLED_F8x16[Char - ' '][i];
//		OLED_WriteData(data);
//        OLED_Buffer[(Line - 1) * 2][(Column - 1) * 8 + i] = data; // 同步到Buffer
//	}
//	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
//	for (i = 0; i < 8; i++)
//	{
//        uint8_t data = OLED_F8x16[Char - ' '][i + 8];
//		OLED_WriteData(data);
//        OLED_Buffer[(Line - 1) * 2 + 1][(Column - 1) * 8 + i] = data; // 同步到Buffer
//	}
//}

//void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
//{
//	uint8_t i;
//	for (i = 0; String[i] != '\0'; i++)
//	{
//		OLED_ShowChar(Line, Column + i, String[i]);
//	}
//}

//uint32_t OLED_Pow(uint32_t X, uint32_t Y)
//{
//	uint32_t Result = 1;
//	while (Y--)
//	{
//		Result *= X;
//	}
//	return Result;
//}

//void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
//{
//	uint8_t i;
//	for (i = 0; i < Length; i++)							
//	{
//		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
//	}
//}

//void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
//{
//	uint8_t i;
//	uint32_t Number1;
//	if (Number >= 0)
//	{
//		OLED_ShowChar(Line, Column, '+');
//		Number1 = Number;
//	}
//	else
//	{
//		OLED_ShowChar(Line, Column, '-');
//		Number1 = -Number;
//	}
//	for (i = 0; i < Length; i++)							
//	{
//		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
//	}
//}

//void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
//{
//	uint8_t i, SingleNumber;
//	for (i = 0; i < Length; i++)							
//	{
//		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
//		if (SingleNumber < 10)
//		{
//			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
//		}
//		else
//		{
//			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
//		}
//	}
//}

//void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
//{
//	uint8_t i;
//	for (i = 0; i < Length; i++)							
//	{
//		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
//	}
//}


/**
  * @brief  OLED显示一个字符（优化版）
  * @note   修改缓冲区后，立即刷新该字符区域
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	uint8_t page = (Line - 1) * 2;       // 计算页地址
	uint8_t col_idx = (Column - 1) * 8;  // 计算列索引
	
	// 1. 更新显存 (上半部分)
	for (i = 0; i < 8; i++)
	{
		OLED_Buffer[page][col_idx + i] = OLED_F8x16[Char - ' '][i];
	}
	
	// 2. 更新显存 (下半部分)
	for (i = 0; i < 8; i++)
	{
		OLED_Buffer[page + 1][col_idx + i] = OLED_F8x16[Char - ' '][i + 8];
	}
	
	// 3. 局部刷新 (只刷新这一个字符的区域：8x16像素)
	// (Line-1)*16 是将行号转换为像素Y坐标
	OLED_Refresh_Area(col_idx, (Line - 1) * 16, 8, 16);
}

/**
  * @brief  OLED显示字符串（优化版）
  * @note   一次性写入所有字符到显存，然后只产生一次I2C通信
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
//	uint8_t j;
	uint8_t page = (Line - 1) * 2;
	uint8_t col_start = (Column - 1) * 8;
	uint8_t char_cnt = 0; // 记录字符个数
	
	// 1. 遍历字符串修改显存
	while (String[char_cnt] != '\0')
	{
		// 防止越界
		if (col_start + char_cnt * 8 >= 128) break;
		
		uint8_t c = String[char_cnt] - ' ';
		
		for (i = 0; i < 8; i++)
		{
			// 写入 Buffer (上半部 & 下半部)
			OLED_Buffer[page][col_start + char_cnt * 8 + i]     = OLED_F8x16[c][i];
			OLED_Buffer[page + 1][col_start + char_cnt * 8 + i] = OLED_F8x16[c][i + 8];
		}
		char_cnt++;
	}
	
	// 2. 批量刷新 (宽度 = 字符数 * 8)
	// 极大的性能提升点：无论字符串多长，只发送一段连续的I2C数据
	OLED_Refresh_Area(col_start, (Line - 1) * 16, char_cnt * 8, 16);
}

/**
  * @brief  OLED次方函数 (保持不变，辅助计算)
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（优化版）
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, j;
	uint8_t page = (Line - 1) * 2;
	uint8_t col_start = (Column - 1) * 8;
	
	for (i = 0; i < Length; i++)							
	{
		// 计算当前位的数字
		uint8_t single_num = Number / OLED_Pow(10, Length - i - 1) % 10;
		uint8_t c = single_num + '0' - ' '; // 转换为字模索引
		
		// 写入显存
		for (j = 0; j < 8; j++)
		{
			OLED_Buffer[page][col_start + i * 8 + j]     = OLED_F8x16[c][j];
			OLED_Buffer[page + 1][col_start + i * 8 + j] = OLED_F8x16[c][j + 8];
		}
	}
	
	// 批量刷新
	OLED_Refresh_Area(col_start, (Line - 1) * 16, Length * 8, 16);
}

/**
  * @brief  OLED显示有符号数字（优化版）
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i, j;
	uint32_t Number1;
	uint8_t page = (Line - 1) * 2;
	uint8_t col_start = (Column - 1) * 8;
	
	// 处理符号位
	uint8_t sign_char = (Number >= 0) ? '+' : '-';
	Number1 = (Number >= 0) ? Number : -Number;
	
	// 1. 写入符号到显存
	uint8_t c_sign = sign_char - ' ';
	for (j = 0; j < 8; j++)
	{
		OLED_Buffer[page][col_start + j]     = OLED_F8x16[c_sign][j];
		OLED_Buffer[page + 1][col_start + j] = OLED_F8x16[c_sign][j + 8];
	}
	
	// 2. 写入数字到显存
	for (i = 0; i < Length; i++)							
	{
		uint8_t single_num = Number1 / OLED_Pow(10, Length - i - 1) % 10;
		uint8_t c = single_num + '0' - ' ';
		
		// 注意列坐标要偏移8像素(因为前面有个符号)
		uint8_t current_x = col_start + 8 + i * 8; 
		
		for (j = 0; j < 8; j++)
		{
			OLED_Buffer[page][current_x + j]     = OLED_F8x16[c][j];
			OLED_Buffer[page + 1][current_x + j] = OLED_F8x16[c][j + 8];
		}
	}
	
	// 3. 批量刷新 (宽度 = (Length + 1) * 8)
	OLED_Refresh_Area(col_start, (Line - 1) * 16, (Length + 1) * 8, 16);
}

/**
  * @brief  OLED显示十六进制数字（优化版）
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, j;
	uint8_t page = (Line - 1) * 2;
	uint8_t col_start = (Column - 1) * 8;
	
	for (i = 0; i < Length; i++)							
	{
		uint8_t single_num = Number / OLED_Pow(16, Length - i - 1) % 16;
		char char_val;
		
		if (single_num < 10) char_val = single_num + '0';
		else char_val = single_num - 10 + 'A';
		
		uint8_t c = char_val - ' ';
		
		for (j = 0; j < 8; j++)
		{
			OLED_Buffer[page][col_start + i * 8 + j]     = OLED_F8x16[c][j];
			OLED_Buffer[page + 1][col_start + i * 8 + j] = OLED_F8x16[c][j + 8];
		}
	}
	
	OLED_Refresh_Area(col_start, (Line - 1) * 16, Length * 8, 16);
}

/**
  * @brief  OLED显示二进制数字（优化版）
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, j;
	uint8_t page = (Line - 1) * 2;
	uint8_t col_start = (Column - 1) * 8;
	
	for (i = 0; i < Length; i++)							
	{
		uint8_t single_num = Number / OLED_Pow(2, Length - i - 1) % 2;
		uint8_t c = single_num + '0' - ' ';
		
		for (j = 0; j < 8; j++)
		{
			OLED_Buffer[page][col_start + i * 8 + j]     = OLED_F8x16[c][j];
			OLED_Buffer[page + 1][col_start + i * 8 + j] = OLED_F8x16[c][j + 8];
		}
	}
	
	OLED_Refresh_Area(col_start, (Line - 1) * 16, Length * 8, 16);
}
/**
  * @brief  OLED显示位图（优化版：写入显存 + 局部刷新）
  * @param  x     起始列 (0~127)
  * @param  y     起始页 (0~7) !!!注意这里是页地址，不是像素坐标
  * @param  width 图像宽度
  * @param  height 图像高度（必须是8的倍数）
  * @param  bmp   位图数据数组
  * @param  mode  1:正常显示, 0:反色显示
  * @retval 无
  */
void OLED_ShowBMP(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bmp, uint8_t mode) 
{
    uint8_t i, j;
    uint8_t page_len = height / 8; // 计算占用多少页
    uint16_t index = 0;
    
    // 1. 将位图数据写入显存缓冲区
    for (i = 0; i < page_len; i++) 
    {
        // 防止页越界
        if (y + i >= 8) break;

        for (j = 0; j < width; j++) 
        {
            // 防止列越界
            if (x + j >= 128) {
                index++; // 即使越界，数据指针也要后移，保持图片结构
                continue;
            }

            uint8_t data = bmp[index++];
            
            // 更新缓冲区 (mode=1原样, mode=0取反)
            OLED_Buffer[y + i][x + j] = (mode == 1) ? data : ~data;
        }
    }

    // 2. 局部刷新屏幕
    // 注意：OLED_Refresh_Area 接受的是像素坐标(0~63)，而此函数的 y 是页地址(0~7)
    // 所以这里传递 y * 8
    OLED_Refresh_Area(x, y * 8, width, height);
}
/**
  * @brief  OLED初始化
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)			//上电延时
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			//端口初始化
	
	OLED_WriteCommand(0xAE);	//关闭显示
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);	//设置显示开始行
	OLED_WriteCommand(0xA1);	//设置左右方向
	OLED_WriteCommand(0xC8);	//设置上下方向
	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭
	OLED_WriteCommand(0xA6);	//设置正常/倒转显示
	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);	//开启显示
		
	OLED_Clear();				//OLED清屏（现在调用的是快速清屏）
}
