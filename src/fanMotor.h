#ifndef _fanMotor_H_FILE
#define _fanMotor_H_FILE

#include <Arduino.h>
#define WIFI_DEBUG_PORT_FANMOTOR
#if defined(WIFI_DEBUG_PORT_FANMOTOR)
#include <WiFi.h>
#endif

#define AVG_NUM 88
#define DS18B20_PORT 21

static volatile uint32_t avgRpm[AVG_NUM];
static volatile uint32_t ptr;
static volatile uint32_t microtrig ;
#if defined(WIFI_DEBUG_PORT_FANMOTOR)
extern WiFiClient client;
#endif
extern int temp10x_set; // 温度乘以10(设定值)
extern int rpm_set; // 转速RPM(设定值)
extern int time_set; //时间设定值
void fanMotorInit();
void setFanMotorSpeed(int s_peed);
void setHeat(uint16_t s_heat);
int getAvgRpm();//获取风扇转速

static bool initial;
bool initDs18b20();
void resetDs18b20();
uint8_t hashDs18b20();
uint16_t getRaw();
int getTemperature10x();
uint8_t ds18b20_read_byte(void);
void ds18b20_write_byte(uint8_t dat);
int getCntPwm();

void THandleTempCtrl(int temp10x_set);
void THandleSpeedCtrl(int rpm_set);

#endif