# Faster-HAL-Library-OLED-Driver  
 [Buffer加速和局部刷新优化] 把江协科技的0.96寸OLED显示屏驱动移植到了HAL库并进行了算法优化，包括新增可以设定字体大小的函数减少字模的存储空间占用，使用Buffer加速和局部刷新优化以增加显示速度。Including newly added functions that allow font size adjustment to reduce the storage space occupied by font patterns, utilizing Buffer acceleration and partial refresh optimization to enhance display speed.

### ✅ 可以设定字体大小
### ✅ 使用Buffer加速  
### ✅ 局部刷新
### ✅ 函数名和调用参数不变

# ！快速
原始方案（无Buffer）：  
假设需要写入1024字节（128×8页）  
实测约0.25ms/byte （100kHz）  
总时间：1024 × 0.25ms ≈ 256ms  
  
Buffer优化方案：  
全屏刷新（8页）耗时  
$T_{page} = T_{start} + 9 \times T_{bit} \times (1_{addr} + 128_{data}) + T_{stop}
$  
@100kHz: 8 × 11.7ms ≈ 93.6ms  
@400kHz: 8 × 2.9ms ≈ 23.2ms  

# 轻松开始
将文件添加到Keil中  
<img width="249" height="123" alt="image" src="https://github.com/user-attachments/assets/fe633014-f505-4f72-bb96-1afc2a5c7195" />      
在你的主函数中包含头文件  
```C    
#include "OLED.h"    
```
即可调用显示函数。  
请一定要记得对显示屏进行初始化  
```C      
OLED_Init();   
```
初始化后，显示屏的PB8引脚将被定义为SCL，PB9引脚将被定义为SDA，你也可以按照自己的需求进行更改  
```C   
#define OLED_W_SCL(x)		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, (GPIO_PinState)(x))      
#define OLED_W_SDA(x)		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, (GPIO_PinState)(x))    
```
若更改引脚需要在OLED_Init()函数中配置如下两处地方  
<img width="778" height="354" alt="image" src="https://github.com/user-attachments/assets/c4b625ce-aa9b-4d86-ad18-25dd6ef13817" />  
防止出错  
需要显示BMP格式的表情包请声明全局变量  
```C      
extern const uint8_t BMP_ICON[][8192];//存放表情包的数组    
```

# 参考文件  
需要江协科技标准库驱动源码可以点击链接：
[江协科技](https://jiangxiekeji.com/tutorial/oled.html)  
