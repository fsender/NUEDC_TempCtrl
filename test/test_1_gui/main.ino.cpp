/*************** FRIENDSHIPENDER 电赛代码 ****************
 * @file main.ino.cpp
 * @author FriendshipEnder (you@domain.com)
 * @brief 
 * 普洛托: 写点啥东西好呐,...
 * 莉莉勾: 这个可是电赛呀, 电 赛 呀!!!!
 * 普洛托: 今天被这个IDE整麻了
 * 普洛托: 都怪你, 甚么烂IDE!!! 居然把新生的学妹, 搞的如此不堪
 * 
 * @version 1.0
 * @date 2022-06-27
 * @copyright Copyright (c) 2022
 * 
 */
#include <Arduino.h>
#define DROPPING FALLING
#include "TFT_eSPI.h"
#include "U8g2_for_TFT_eSPI.h"

#include "temperature.h"
#include "heat.h"
#include "fanMotor.h"
#include "resource.h"

#define KEY_OK     4
#define KEY_LEFT   5
#define KEY_RIGHT  9
#define KEY_UP     8
#define KEY_DOWN  20
static TFT_eSPI ips = TFT_eSPI();                 // LGFXのインスタンスを作成。
static U8g2_for_TFT_eSPI cn;
/** @brief drawStatus: 显示温控内核运行状态
 *  @param temp  温度, 单位摄氏度
 *  @param rpm   转速 单位转每分钟
 *  @param ampere 电流, 单位mA
 */
void drawStatus(int doffset, int temp, int rpm, int ampere, int setval);
int getNum(int initial);
//void trigger_fun();

/** @brief  运行模式 0 温度控制   1 转速控制   2 温度定时   3 转速定时
 *  0 温度控制: 按上下键加减温度  1 转速控制: 按上下键加减转速
 * 按确定按键进入键盘输入 长按确定键急停
 * 
 *  2 温度定时: 按上下按键设置时间, 单位秒, 按确定键设置时间, 设置好了就可以进入定时模式
 * 默认为关闭状态
 * 设定好了之后 -> 等待调温 -> 开始定时 -> 持续时间结束之后声光提示 -> 关闭温度+风扇控制器
 * 按确定按键进入键盘输入 长按确定键急停
 * 
 *  3 转速定时: 按上下按键设置时间, 单位秒, 按确定键设置时间, 设置好了就可以进入定时模式
 * 
 *  4 手动模式: 可以自己调整转速和加热器功率, 长按急停
 */
uint8_t runMode = 0; 
const char * runModeString[6] = {
  "恒定温度控制","恒定转速控制","温度定时","转速定时","手动设置加热功率","手动设置电机功率"
};
uint8_t getBtn();
int temp10x; //温度乘以10
int rpm; // 转速RPM
int microamp; // 电流毫安

int temp10x_set = 600; // 温度乘以10(设定值)
int rpm_set = 1000; // 转速RPM(设定值)
int time_set = 0; //时间设定值

int baseHeat_set = 32768;   //加热功率设定值
int baseMotor_set = 32768;  //点击功率设定值

void setup(){
  Serial.begin(115200);
  pinMode(KEY_OK,INPUT_PULLUP); //确定
  pinMode(KEY_LEFT,INPUT_PULLUP); //左
  pinMode(KEY_UP,INPUT_PULLUP); //上
  pinMode(KEY_RIGHT,INPUT_PULLUP); //右
  pinMode(KEY_DOWN,INPUT_PULLUP); //下
  ips.begin();
  ips.setRotation(3);
  ips.fillScreen(0x0000); //初始黑色
  cn.begin(ips);
  cn.setFont(u8g2_font_wqy12_t_gb2312);
  cn.setBackgroundColor(0);//背景颜色: 黑色
  ips.drawBitmap(33,15,initLogo,92,48,0xffff);  //显示logo, ,团队Logo
  ips.fillScreen(0x0000); //初始黑色
  time_set = 0;
}
void loop(){
  int valSet;
  if(runMode == 0) valSet = temp10x_set;
  if(runMode == 1) valSet = rpm_set;
  if(runMode == 2) valSet = time_set;
  if(runMode == 3) valSet = time_set;
  if(runMode == 4) valSet = baseHeat_set;
  if(runMode == 5) valSet = baseMotor_set;


  temp10x = rand()%10000;
  rpm = rand()%10000;
  microamp = rand()%10000;
  drawStatus(0,temp10x,rpm,microamp,valSet);
  uint8_t got = getBtn();
  if(got) Serial.println(got);
  switch(getBtn()){
    case KEY_OK:
    {
      while(getBtn() == KEY_OK) yield();
      delay(10); //去抖
      if(runMode == 0) temp10x_set = getNum(valSet)*10;
      if(runMode == 1) rpm_set = getNum(valSet);
      if(runMode == 2) time_set = getNum(valSet);
      if(runMode == 3) time_set = getNum(valSet);
      if(runMode == 4) baseHeat_set = getNum(valSet);
      if(runMode == 5) baseMotor_set = getNum(valSet);
    }
    break;
    
    case KEY_UP:
    {
      while(getBtn() == KEY_UP) yield();
      delay(10); //去抖
      if(runMode == 0) temp10x_set = valSet+1;
      if(runMode == 1) rpm_set = valSet+1;
      if(runMode == 2) time_set = valSet+1;
      if(runMode == 3) time_set = valSet+1;
      if(runMode == 4) baseHeat_set = valSet+1;
      if(runMode == 5) baseMotor_set = valSet+1;

      if(temp10x_set < 500) temp10x_set = 500;  //最小的允许设定值
      if(temp10x_set > 750) temp10x_set = 750;  //最大的允许设定值
      if(rpm_set <   300) rpm_set =   300;      //最小的允许设定值
      if(rpm_set > 60000) rpm_set = 60000;      //最大的允许设定值
      if(baseHeat_set <0 ) baseHeat_set = 0;
      if(baseHeat_set >65535 ) baseHeat_set =65535;
      if(baseMotor_set <0 ) baseMotor_set = 0;
      if(baseMotor_set >65535 ) baseMotor_set =65535;
    }
    break;
    case KEY_DOWN:
    {
      while(getBtn() == KEY_DOWN) yield();
      delay(10); //去抖
      if(runMode == 0) temp10x_set = valSet-1;
      if(runMode == 1) rpm_set = valSet-1;
      if(runMode == 2) time_set = valSet-1;
      if(runMode == 3) time_set = valSet-1;
      if(runMode == 4) baseHeat_set = valSet-1;
      if(runMode == 5) baseMotor_set = valSet-1;

      if(temp10x_set < 500) temp10x_set = 500;  //最小的允许设定值
      if(temp10x_set > 750) temp10x_set = 750;  //最大的允许设定值
      if(rpm_set <   300) rpm_set =   300;      //最小的允许设定值
      if(rpm_set > 60000) rpm_set = 60000;      //最大的允许设定值
      if(baseHeat_set <0 ) baseHeat_set = 0;
      if(baseHeat_set >65535 ) baseHeat_set =65535;
      if(baseMotor_set <0 ) baseMotor_set = 0;
      if(baseMotor_set >65535 ) baseMotor_set =65535;
    }
    break;
    case KEY_LEFT:
    {
      for(int i = 0;i<13;i++){
        drawStatus(i*i+1,temp10x,rpm,microamp,valSet);
        Serial.println("run drawStatus1");
      }
      if(runMode) runMode--;
      else runMode = 5;
    }
    break;
    case KEY_RIGHT:
    {
      if(runMode<5) runMode++;
      else runMode = 0;
      for(int i = 12;i>=0;i--){
        drawStatus(i*i+1,temp10x,rpm,microamp,valSet);
        Serial.println("run drawStatus2");
      }
    }
    break;
  }
}
void drawStatus(int doffset, int temp, int rpm, int ampere, int setval){
  cn.setCursor(doffset,11);
  cn.setForegroundColor(ips.color565(255,64,0));
  ips.fillRect(0,0,160,12,0x0000); //初始黑色
  cn.printf("模式:%s",runModeString[runMode]);
  cn.setCursor(doffset,23);
  cn.setForegroundColor(ips.color565(64,255,0));
  ips.fillRect(0,12,160,12,0x0000); //初始黑色
  cn.printf("温度:%d.%d 摄氏度",temp/10,temp%10);
  cn.setCursor(doffset,35);
  cn.setForegroundColor(ips.color565(255,0,64));
  ips.fillRect(0,24,160,12,0x0000); //初始黑色
  cn.printf("转速:-%d RPM",rpm);
  cn.setCursor(doffset,47);
  cn.setForegroundColor(ips.color565(0,255,192));
  ips.fillRect(0,36,160,12,0x0000); //初始黑色
  cn.printf("电流:%d mA",ampere);
  cn.setCursor(doffset,59);
  cn.setForegroundColor(ips.color565(64,0,255));
  ips.fillRect(0,48,160,12,0x0000); //初始黑色
  cn.printf("设定值: %d",setval);
  //cn.setCursor(doffset,71);
  //cn.setForegroundColor(ips.color565(64,0,255));
  //ips.fillRect(0,60,160,12,0x0000); //初始黑色
  //cn.print(runModeString[runMode]);
}

uint8_t getBtn(){
  //if(!digitalRead(KEY_OK)) return KEY_OK;
  if(!digitalRead(KEY_LEFT)) return KEY_LEFT;
  if(!digitalRead(KEY_RIGHT)) return KEY_RIGHT;
  if(!digitalRead(KEY_UP)) return KEY_UP;
  if(!digitalRead(KEY_DOWN)) return KEY_DOWN;
  return 0;
}
int getNum(int initial){
  return rand()%10000;
}
/*
void trigger_fun(){
  static uint32_t microtrig = 0;
  if(millis() - microtrig >= 100){
    Serial.println("s");
    microtrig = millis();
  }
}*/