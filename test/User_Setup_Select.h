#ifndef USER_SETUP_LOADED 

#include <User_Setups/Setup43_ST7735.h>            // Setup file for ESP8266 & ESP32 configured for my ST7735S 80x160
//#include <User_Setups/Setup18_ST7789.h>            // Setup file for ESP8266 configured for ST7789

#endif // USER_SETUP_LOADED

#define TFT_BGR 0   // Colour order Blue-Green-Red
#define TFT_RGB 1   // Colour order Red-Green-Blue
#if defined (ST7735_DRIVER)
     #include <TFT_Drivers/ST7735_Defines.h>
     #define  TFT_DRIVER 0x7735
#elif defined (ST7789_DRIVER)
     #include "TFT_Drivers/ST7789_Defines.h"
     #define  TFT_DRIVER 0x7789
#endif