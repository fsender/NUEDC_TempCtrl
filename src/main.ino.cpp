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
 * WiFi 功能说明
 * TCP连接, 连接后可以控制部分参数
 * m : 模式设置
 *   M0 : 急停
 *   M1 : 恒定温度控制
 *   M2 : 恒定转速控制
 *   M3 : 温度定时
 *   M4 : 转速波形
 *   M5 : 手动设置加热功率
 *   M6 : 手动设置电机功率
 * s : 设置参数值(温度单位: 0.1摄氏度)
 *   s620 : 恒定温度控制模式下, 把恒温温度设置为62摄氏度
 *          恒定转速控制模式下, 转速设置为1200RPM
 *          温度定时模式下, 设置目标温度为62摄氏度
 *          波形模式下, s1: 正弦 s2: 方波 s3: 三角波 s4: 锯齿波
 *          手动加热功率模式下, 设置加热PWM为620(范围0 - 4095)
 *          手动设置电机功率, 设置电机PWM为1200(范围 -4095 - 4095)
 * n : 时间设置
 *   n30:   在温度定时模式下, 设置保温时间为30秒, 并开始
 *          在波形模式下, 设置周期为30秒
 * e : 保温时的温度误差范围设置(单位为0.1摄氏度)
 *   e5:    设置误差范围为±0.5摄氏度
 * a : 设置报警电流, 单位毫安
 * p : 设置Pid控制内核的p参数 p0.1
 * d : 设置Pid控制内核的d参数 d0.1
 */
#define WIFI_DEBUG_PORT
#include <Arduino.h>
#if defined(WIFI_DEBUG_PORT)
#include <WiFi.h>
#endif
#include "TFT_eSPI.h"
#include "U8g2_for_TFT_eSPI.h"

#include "fanMotor.h"
#include "microAmp.h"
#include "resource.h"

#define KEY_OK     4
#define KEY_LEFT   9
#define KEY_RIGHT  5
#define KEY_UP    20
#define KEY_DOWN   8

static TFT_eSPI ips = TFT_eSPI();                 // LGFXのインスタンスを作成。
static U8g2_for_TFT_eSPI cn;
#if defined(WIFI_DEBUG_PORT)
WiFiClient client;
WiFiServer sclient;
#endif

int temp10x; //温度乘以10
int rpm; // 转速RPM
int microamp; // 电流毫安

int temp10x_set = 400; // 温度乘以10(设定值)
int rpm_set = 1000; // 转速RPM(设定值)

int waveType = 0; //正弦波
int wavePeriod = 10000; //周期, 单位是毫秒
int waveAmplitude = 2048; //振幅

int baseHeat_set = 0;   //加热功率设定值
int baseMotor_set = 0;  //点击功率设定值
int tempHistory[160];
int historyCursor = 0;
int rpmHistory[160];
int ampOverflow = 800000;
uint32_t bootMillis = 0;  // 为了防止在加速时触发此检测

int time_set = 0; //时间设定值
uint32_t timStartHeatMillis = 0; //记录开始保温的时间
uint32_t timStartMillis = 0; //记录开始保温的时间
uint16_t timStartTemp = 0; //记录起始温度
uint16_t timTargetTemp = 0; //记录保温温度
uint8_t  timMode = 0; //记录当前保温状态

uint8_t  showMap = 0; //0:不显示图像  1:显示转速图像   2:显示温度图像
uint32_t waveRefreshMillis = 0; //只在正弦模式下使用, 用于计时刷新

/** @brief drawStatus: 显示温控内核运行状态
 *  @param temp  温度, 单位摄氏度
 *  @param rpm   转速 单位转每分钟
 *  @param ampere 电流, 单位mA
 */
void drawStatus(int doffset, int temp, int rpm, int ampere, int setval);
void draw_graph_rpm(int *dat, int rangeMin, int rangeMax, int setRpm = 2147483647);
void draw_graph_temp(int *dat, int rangeMin, int rangeMax, int setTemp = 2147483647);
int menuSelect();
void emergencyStop(int reason); //急停

//此处代码只是为了保温函数
  static int e = 10; //正负1度
  static int lastErr = 0;
  static int maxError = 0;
  static int outTemp = 0;
void showKeeperInfo(); //新增: 显示保温状态

/** @brief 键盘输入数字
 * @param initial 初值
 * @return int 输入的数值
 */
int getNum(int initial, const char *assigns = nullptr);
uint8_t runMode = 5; 
const char * runModeString[6] = {
  "恒定温度控制","恒定转速控制","温度定时","转速波形","手动设置加热功率","手动设置电机功率"
};
const char * timStatusString[3] = {
  "未保温或保温完毕", "正在加热", "正在保温"
};
const char * waveTypeString[5] = {
  "无波形","正弦波", "方波", "三角波","锯齿波"
};
const char * runModeStringEn[6] = {
  "Temperature","Speed","Temp timing","Speed wave","Manual heat","Manual Speed"
};
uint8_t getBtn();

void setup(){
  //Serial.begin(115200);
  ips.begin();
  ips.setRotation(1);
  ips.fillScreen(0x0000); //初始黑色
  ips.drawBitmap(33,15,initLogo,92,48,0xffff);  //显示logo, ,团队Logo
  ips.setCursor(0,0);
  pinMode(12,OUTPUT);
  digitalWrite(12,1);
  fanMotorInit();
  ips.setTextColor(TFT_CYAN);
  ips.println("Fan and Motor Init.\n");
  while(initDs18b20()) {
    //Serial.println("FATEL ERROR: DS18B20 NOT FOUND!!!!!\n");
    ips.setCursor(0,8);
    ips.setTextColor(TFT_RED);
    ips.println("ERROR: DS18B20 NOT FOUND!\n");
    delay(100);
  }
  ips.setTextColor(0xfc00); //橙色
  ips.println("Connecting to WiFi...");
  pinMode(KEY_OK,INPUT_PULLUP); //确定
  digitalWrite(12,0);
#if defined(WIFI_DEBUG_PORT)
  WiFi.mode(WIFI_STA);
  WiFi.begin("iQOONeo5Lite","12345679");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    if(digitalRead(KEY_OK) == 0) break;
  }
  if(WiFi.status() == WL_CONNECTED){
  ips.setTextColor(TFT_MAGENTA);
  ips.println(WiFi.localIP());
  ips.setTextColor(TFT_GREEN);
  ips.println("Waiting for client connect...");
  sclient.begin(777);
  while (!sclient.hasClient()) {
    delay(100);
    if(digitalRead(KEY_OK) == 0) break;
  }
  if(sclient.hasClient()) client = sclient.available();
  ips.print("Connected !");
  }
#endif
  digitalWrite(12,1);
  delay(300);
  digitalWrite(12,0);
  while(millis() <= 1000);
  pinMode(KEY_LEFT,INPUT_PULLUP); //左
  pinMode(KEY_UP,INPUT_PULLUP); //上
  pinMode(KEY_RIGHT,INPUT_PULLUP); //右
  pinMode(KEY_DOWN,INPUT_PULLUP); //下
  initMicroAmp();
  cn.begin(ips);
  cn.setFont(u8g2_font_wqy12_t_gb2312);
  cn.setBackgroundColor(0);//背景颜色: 黑色
  for(uint8_t i = 0;i < 160;i++){
    tempHistory[i] = 250;
    rpmHistory[i] = 1000;
  }
  ips.fillScreen(0x0000); //初始黑色
  
  while(getTemperature10x() >= 850){
    cn.setCursor(0,11);
    cn.setForegroundColor(ips.color565(255,64,0));
    cn.print("欢迎使用温控系统! 感谢合宙LuatOS提供");
    cn.setCursor(0,23);
    cn.setForegroundColor(ips.color565(64,255,0));
    cn.print("左右摇动摇杆来切换温控,转速控制等6种模式");
    cn.setCursor(0,35);
    cn.setForegroundColor(ips.color565(128,0,255));
    cn.print("上下摇动摇杆键选择查看温度/转速曲线图像");
    cn.setCursor(0,47);
    cn.setForegroundColor(ips.color565(255,160,192));
    cn.print("按摇杆键设置数值, 长按急停");
    cn.setCursor(0,59);
    cn.setForegroundColor(ips.color565(0,255,128));
    cn.print("版权所有 (C) FriendshipEnder 2022.7.4");
    cn.setCursor(0,71);
    cn.setForegroundColor(ips.color565(128,128,255));
    cn.print("鹿图斯, 辛苦了, 你的C3内心我会继续榨干的");
    //按左右键来切换模式
    //按上下键选择查看温度/转速曲线图像
    //按确定键
  }
  digitalWrite(12,1);
  delay(50);
  digitalWrite(12,0);
}
void loop(){
  
  int valSet;
  if(runMode == 0) {
    valSet = temp10x_set;
    THandleTempCtrl(temp10x_set);
    delay(350);
  }
  else if(runMode == 1) {
    valSet = rpm_set;
    THandleSpeedCtrl(rpm_set);
  }
  else if(runMode == 2) { //保温状态
    if(timMode == 2 && timStartMillis && millis() - timStartMillis >= time_set*1000) {
      setFanMotorSpeed(2147483647);
      setHeat(0);
        timMode = 0;
        digitalWrite(12,HIGH);
        delay(500);
        digitalWrite(12,LOW);
        delay(50);
        digitalWrite(12,HIGH);
        delay(50);
        digitalWrite(12,LOW);
        //----退出保温状态----需要声光提示----
        time_set = 0;
    }
    if(timMode >= 1) {
      THandleTempCtrl(timTargetTemp);
      showKeeperInfo();
      delay(320);
    }
    if(timMode == 1 && ((temp10x >= timTargetTemp && timStartTemp < timTargetTemp)
      || (temp10x <= timTargetTemp && timStartTemp > timTargetTemp))) {
        timMode = 2;
        timStartMillis = millis();
        digitalWrite(12,HIGH);
        delay(50);
        digitalWrite(12,LOW);
        delay(50);
        digitalWrite(12,HIGH);
        delay(500);
        digitalWrite(12,LOW);
        //----进入保温状态----需要声光提示----
    }
    if(timMode == 2) valSet = time_set - (millis() - timStartMillis)/1000;
    else valSet = time_set;
  }
  else if(runMode == 3) {
    valSet = wavePeriod/1000;
    if(waveType == 1) { //正弦
      double v = sin(double(millis()%wavePeriod)*TWO_PI/wavePeriod);
      setFanMotorSpeed(v*waveAmplitude);
    }
    else if(waveType == 2) { //方波
      if(millis()%wavePeriod > wavePeriod/2) setFanMotorSpeed(-waveAmplitude);
      else setFanMotorSpeed(waveAmplitude);
    }
    else if(waveType == 3) { //三角
      if(millis()%wavePeriod > wavePeriod/2) {
        int d = millis()%wavePeriod - wavePeriod/2;
        int dmax = wavePeriod/2;
        setFanMotorSpeed(waveAmplitude-d*waveAmplitude*2/dmax);
      }
      else {
        int d = millis()%wavePeriod;
        int dmax = wavePeriod/2;
        setFanMotorSpeed(d*waveAmplitude*2/dmax-waveAmplitude);
      }
    }
    else if(waveType == 4) { //锯齿
      setFanMotorSpeed((millis()%wavePeriod)*waveAmplitude*2/wavePeriod-waveAmplitude);
    }
  }
  else if(runMode == 4) {
    setFanMotorSpeed(355);
    valSet = baseHeat_set;
  }
  else if(runMode == 5) {
    valSet = baseMotor_set;
  }
  if(runMode != 2){
    //所有相关参数归零
    time_set = 0;
    timStartTemp = 0;
    timStartMillis = 0;
    timStartHeatMillis = 0;
  }

  rpm = getAvgRpm();
  temp10x = getTemperature10x();
  if(temp10x >= 840){
    emergencyStop(1);
  }
  if(runMode != 3){
    tempHistory[historyCursor] = temp10x;
    rpmHistory[historyCursor] = rpm;
    if(historyCursor<159) historyCursor++;
    else historyCursor = 0;
  }
  else if(millis() - waveRefreshMillis > 500){
    tempHistory[historyCursor] = temp10x;
    rpmHistory[historyCursor] = rpm;
    if(historyCursor<159) historyCursor++;
    else historyCursor = 0;
    waveRefreshMillis += 500;
    drawStatus(0,temp10x,rpm,microamp,valSet);
  }
  microamp = getMicroAmp();
  if(microamp > ampOverflow && millis() - bootMillis > 1000){
    emergencyStop(microamp);
  }
  if(runMode != 3 || showMap == 0)
    drawStatus(0,temp10x,rpm,microamp,valSet);
  uint8_t got = getBtn();
  if(got && client.available()) client.readString();
  switch(got){
    case KEY_OK:
    {
      uint32_t beginPressMillis = millis();
      while(getBtn() == KEY_OK) {
        if(millis() - beginPressMillis > 1000){ //触发长按
          emergencyStop(0);
          beginPressMillis = 0;
        }
      }
      digitalWrite(12,1);
      delay(15); //去抖
      digitalWrite(12,0);
      if(!beginPressMillis) break;
      showMap = 0;
      if(runMode == 0) {
        temp10x_set = getNum(valSet,"输入10倍温度(50度输入500)");
        if(temp10x_set > 800) temp10x_set = 800;  //最大的允许设定值
        if(temp10x_set < 300) temp10x_set = 300;  //最大的允许设定值
        bootMillis = millis();
      }
      if(runMode == 1) {
        rpm_set = getNum(valSet,"输入转速, 负数为反转");
        if(rpm_set > 6000) rpm_set = 6000;      //最大的允许设定值
        if(rpm_set > 0 && rpm_set < 300) rpm_set = 300;      //最大的允许设定值
        if(rpm_set <-6000) rpm_set =-6000;      //最大的允许设定值
        if(rpm_set < 0 && rpm_set > -300) rpm_set = -300;      //最大的允许设定值
        bootMillis = millis();
      }
      if(runMode == 2) { //设置保温时间等数据
        timTargetTemp = getNum(500,"设置保温温度(50度输入500)");
        time_set = getNum(10,"设置保温时间, 单位秒");
        if(timTargetTemp < 200 || timTargetTemp > 800 || !time_set){
        //不合理的温度
          ips.fillRect(0,32,160,20,TFT_BROWN);
          cn.setBackgroundColor(TFT_BROWN);
          cn.drawUTF8(26,48,"设置的数值超过范围!");
          cn.setBackgroundColor(0x0000);
          while(!getBtn());
          delay(10);
        }
        timMode = 1;
        timStartHeatMillis = millis();
        timStartMillis = 0;
        timStartTemp = temp10x;
        bootMillis = millis();
      }
      if(runMode == 3) { //设置波形, 周期, 最大转速
        const char *waveTypeStr[4] = {
          "正弦波","方波","三角波","锯齿波"
        };
        waveType = menuSelect();
        if(waveType){
          char s[40] = "输入";
          strcat(s,waveTypeStr[waveType-1]);
          strcat(s,"周期, 单位: 秒");
          wavePeriod = getNum(valSet,s)*1000;
          waveAmplitude = getNum(waveAmplitude,"设置波形幅度");
          if(waveAmplitude<300) waveAmplitude = 300;
          if(waveAmplitude>4096) waveAmplitude = 4096;
        }
        else setFanMotorSpeed(2147483647);
        if(wavePeriod > 300000 || wavePeriod < 1999){
          ips.fillRect(0,32,160,20,TFT_BROWN);
          cn.setBackgroundColor(TFT_BROWN);
          cn.drawUTF8(26,48,"设置的数值超过范围!");
          cn.setBackgroundColor(0x0000);
          while(!getBtn());
          delay(10);
          waveType = 0;
          wavePeriod = 0;
          //不合理
        }
        else waveRefreshMillis = millis();
        bootMillis = millis();
      }
      if(runMode == 4) {
        baseHeat_set = getNum(valSet,"输入加热强度");
        if(baseHeat_set >4095 ) baseHeat_set = 4095;
        if(baseHeat_set <0 ) baseHeat_set = 0;
        setFanMotorSpeed(355);
        setHeat(baseHeat_set);
        bootMillis = millis();
      }
      if(runMode == 5) {
        baseMotor_set = getNum(valSet,"输入电机输出强度");
        if(baseMotor_set >  4095 ) baseMotor_set =  4095;
        if(baseMotor_set < -4095 ) baseMotor_set = -4095;
        setFanMotorSpeed(baseMotor_set);
        bootMillis = millis();
      }
    }
    break;
    
    case KEY_UP:
    {
      while(getBtn() == KEY_UP) yield();
      digitalWrite(12,1);
      delay(15); //去抖
      digitalWrite(12,0);
      if(runMode < 3) {
        if( showMap == 2) showMap =0;
        else  showMap ++;
      }
      if(runMode == 3) {
        showMap = !showMap;
        if(showMap) drawStatus(0,temp10x,rpm,microamp,valSet);
      }
      if(runMode == 4) {
        baseHeat_set = valSet+100;
        if(baseHeat_set >4095 ) baseHeat_set = 4095;
        setHeat(baseHeat_set);
      }
      if(runMode == 5) {
        baseMotor_set = valSet+100;
        if(baseMotor_set >  4095 ) baseMotor_set =  4095;
        setFanMotorSpeed(baseMotor_set);
      }
    }
    break;
    case KEY_DOWN:
    {
      while(getBtn() == KEY_DOWN) yield();
      digitalWrite(12,1);
      delay(15); //去抖
      digitalWrite(12,0);
      if(runMode < 3) {
        if( !showMap ) showMap =2;
        else  showMap --;
      }
      if(runMode == 3) {
        showMap = !showMap;
        if(showMap) drawStatus(0,temp10x,rpm,microamp,valSet);
      }
      if(runMode == 4) {
        baseHeat_set = valSet-100;
        if(baseHeat_set < 0 ) baseHeat_set = 0;
        setHeat(baseHeat_set);
      }
      if(runMode == 5) {
        baseMotor_set = valSet-100;
        if(baseMotor_set < -4095 ) baseMotor_set = -4095;
        setFanMotorSpeed(baseMotor_set);
      }

    }
    break;
    case KEY_LEFT:
      digitalWrite(12,1);
        setHeat(0);
      delay(15);
      digitalWrite(12,0);
      showMap = 0;
    {
      for(int i = 0;i<13;i++){
        drawStatus(i*i+1,temp10x,rpm,microamp,valSet);
        //Serial.println("run drawStatus1");
      }
      if(runMode) runMode--;
      else runMode = 5;
    }
    bootMillis = millis();
    break;
    case KEY_RIGHT:
      digitalWrite(12,1);
        setHeat(0);
      delay(15);
      digitalWrite(12,0);
      showMap = 0;
    {
      if(runMode<5) runMode++;
      else runMode = 0;
      if(runMode == 0) valSet = temp10x_set;
      else if(runMode == 1) valSet = rpm_set;
      else if(runMode == 2) valSet = time_set;
      else if(runMode == 3) valSet = wavePeriod/1000;
      else if(runMode == 4) valSet = baseHeat_set;
      else if(runMode == 5) valSet = baseMotor_set;
      for(int i = 12;i>=0;i--){
        drawStatus(i*i+1,temp10x,rpm,microamp,valSet);
        //Serial.println("run drawStatus2");
      }
    }
    bootMillis = millis();
    break;
  }
  if(client.available()){
    char rd = client.peek(); //dummy
    if(rd == 'm'){
      client.read();
      int cio = client.parseInt();
      if(cio == 0){
        client.printf("Syatem is paused.\n");
        emergencyStop(0);
      }
      else if(cio >0 && cio <7) runMode = cio-1;
      client.printf("Run Mode was set to %s\n",runModeString[runMode]);
      if(runMode == 0) valSet = temp10x_set;
      else if(runMode == 1) valSet = rpm_set;
      else if(runMode == 2) valSet = time_set;
      else if(runMode == 3) valSet = wavePeriod/1000;
      else if(runMode == 4) valSet = baseHeat_set;
      else if(runMode == 5) valSet = baseMotor_set;
      for(int i = 12;i>=0;i--){
        drawStatus(i*i+1,temp10x,rpm,microamp,valSet);
        //Serial.println("run drawStatus2");
      }
      bootMillis = millis();
    }
    else if(rd == 'a'){
      client.read();
      ampOverflow =  client.parseInt();
      if(ampOverflow< 10) ampOverflow = 10;
      client.printf("Maximum protection current %d mA\n",ampOverflow);
      ampOverflow*=1000;
    }
    else if(rd == 'e'){
      client.read();
      e =  client.parseInt();
      if(e< 1) e = 1;
        client.printf("Max ereror was set to %d.%d degree.\n",e/10,e%10);
    }
    else if(rd == 'n' && runMode == 2 && timMode == 0){
      client.read();
      time_set = client.parseInt();
      if(time_set>0){
        client.print("Keeping time set to ");
        client.print(time_set);
        client.print(" and temp:");
        client.print(timTargetTemp/10);
        client.print(".");
        client.print(timTargetTemp%10);
        client.println("oC");
        client.print("Start heeating.");
        timMode = 1;
        timStartHeatMillis = millis();
        timStartMillis = 0;
        timStartTemp = temp10x;
        bootMillis = millis();
      }
    }
    else if(rd == 's' && runMode == 2 && timMode == 0) {
      client.read();
      timTargetTemp = client.parseInt();
      client.print("updated Temperature keeper Setting val: ");
      client.println(timTargetTemp);
    }
  }
}

void analyzeBZ(int *dat160, int &_min, int &_max){
  _min = 2147483647;
  _max = -2147483648;
  for(int i = 0; i<160;i++){
    if(dat160[i]>_max) _max = dat160[i];
    if(dat160[i]<_min) _min = dat160[i];
  }
}

void drawStatus(int doffset, int temp, int rpm, int ampere, int setval){
  //client.printf("模式:%s 温度:%d.%d 摄氏度 转速:-%d RPM 电流:%d mA\n",
  //  runModeString[runMode],temp/10,temp%10,rpm,ampere);
  if(!showMap){ //0: 关闭图形
    cn.setCursor(doffset,11);
    cn.setForegroundColor(ips.color565(255,64,0));
    ips.fillRect(0,0,160,12,0x0000); //初始黑色
    cn.printf("模式: %s",runModeString[runMode]);
    cn.setCursor(doffset,23);
    cn.setForegroundColor(ips.color565(64,255,0));
    ips.fillRect(0,12,160,12,0x0000); //初始黑色
    cn.printf("温度: %d.%d 摄氏度",temp/10,temp%10);
    cn.setCursor(doffset,35);
    cn.setForegroundColor(ips.color565(255,0,64));
    ips.fillRect(0,24,160,12,0x0000); //初始黑色
    cn.printf("转速: %d RPM",rpm); //此处说明之前已经反转模式了
    cn.setCursor(doffset,47);
    cn.setForegroundColor(ips.color565(0,255,192));
    ips.fillRect(0,36,160,12,0x0000); //初始黑色
    if(ampere<0){
      ampere = -ampere;
      cn.printf("电流: -%d.%03d mA",ampere/1000,ampere%1000);
      ampere = -ampere;
    }
    else cn.printf("电流: %d.%03d mA",ampere/1000,ampere%1000);
    cn.setCursor(doffset,59);
    cn.setForegroundColor(ips.color565(160,128,255));
    ips.fillRect(0,48,160,32,0x0000); //初始黑色
    if(runMode == 2){ 
      cn.printf("设定温度:%d, 剩余时间:%d",timTargetTemp/10,setval);
      if(!doffset){
        cn.setCursor(doffset,71);
        cn.setForegroundColor(ips.color565(255,255,0));
        cn.print("保温状态: ");
        cn.print(timStatusString[timMode]);
      }
    }
    else {
      cn.printf("设定值: %d",setval);
      if(runMode == 3 && !doffset){
        cn.setCursor(doffset,71);
        cn.setForegroundColor(ips.color565(255,255,0));
        cn.print("输出波形: ");
        cn.print(waveTypeString[waveType]);
      }
    }
  }
  else if(showMap == 1){// 显示一条转速
    int rangeMin, rangeMax;
    analyzeBZ(rpmHistory, rangeMin, rangeMax);
    if(runMode == 1){
      if(rpm_set > rangeMax) rangeMax = rpm_set;
      else if(rpm_set < rangeMin) rangeMin = rpm_set;
    }
    //client.printf("rangeMin %d, rangeMax %d\n",rangeMin,rangeMax);
    /*if(rangeMax <80){
        rangeMin = 0;
        rangeMax = 79;
      }
      else if(rangeMin && rangeMax / rangeMin >5){
        rangeMin = 0;
        rangeMax = rangeMax+10;
      }
      else */
      if(rangeMax - rangeMin <80){
        int cursorCentre = (rangeMax+rangeMin)/2;
        rangeMin = cursorCentre-40;
        rangeMax = cursorCentre+39;
      }
      draw_graph_rpm(rpmHistory,rangeMin,rangeMax,(runMode == 1)?rpm_set:2147483647);
    if(runMode == 1) delay(50);
    //delay(100);
  }
  else if(showMap == 2){// 显示一条温度 80像素:可以显示那个8摄氏度的变化范围, 可以自动变化显示范围
    int rangeMin, rangeMax;
    analyzeBZ(tempHistory, rangeMin, rangeMax);
    //client.printf("rangeMin %d, rangeMax %d\n",rangeMin,rangeMax);
    if(runMode == 0){
    if(max(rangeMax,temp10x_set) - min(rangeMin,temp10x_set) <80){
      int cursorCentre = (max(rangeMax,temp10x_set)+min(rangeMin,temp10x_set))/2;
      rangeMin = cursorCentre-39;
      rangeMax = cursorCentre+40;
    }
    else if(temp10x_set >rangeMax ) rangeMax = temp10x_set;
    else if(temp10x_set <rangeMin ) rangeMin = temp10x_set;
    draw_graph_temp(tempHistory,rangeMin,rangeMax,temp10x_set);
    }
    else if(runMode == 2 && timMode){
    if(max(rangeMax,(int)timTargetTemp) - min(rangeMin,(int)timTargetTemp) <80){
      int cursorCentre = (max(rangeMax,(int)timTargetTemp)+min(rangeMin,(int)timTargetTemp))/2;
      rangeMin = cursorCentre-39;
      rangeMax = cursorCentre+40;
    }
    else if(timTargetTemp >rangeMax ) rangeMax = timTargetTemp;
    else if(timTargetTemp <rangeMin ) rangeMin = timTargetTemp;
    draw_graph_temp(tempHistory,rangeMin,rangeMax,timTargetTemp);
    }
    else{
    if(rangeMax-rangeMin<80){
      int cursorCentre =(rangeMax+rangeMin)/2;
      rangeMin = cursorCentre-39;
      rangeMax = cursorCentre+40;
    }
    draw_graph_temp(tempHistory,rangeMin,rangeMax);
    }
//cn.setCursor(doffset,19);
//cn.setForegroundColor(ips.color565(255,64,0));
//cn.printf("模式:%s",runModeString[runMode]);
    //delay(100);
    if(runMode == 1) delay(30);
  }
}

void draw_graph_temp(int *dat, int rangeMin, int rangeMax, int setTemp){
  ips.fillScreen(0x0000); //初始黑色
  ips.setCursor(30,0);
  for(int i = 1;i < 5;i++)
    ips.drawFastHLine(0,i*16,160,0x30c);//0x30c用于显示背景网格, 0x618用于显示设定值
  for(int i = 0;i < 10;i++)
    ips.drawFastVLine(i*16+15-(historyCursor%16),0,160,0x30c);//0x30c用于显示背景网格, 0x618用于显示设定值
  ips.setTextColor(TFT_DARKGREY);
  ips.print("Temperature Wave");
  int vpos = map(dat[(historyCursor+159)%160],rangeMin,rangeMax,79,0);
  if(vpos>76) vpos = 76;
  else if(vpos<4) vpos = 4;
  if(setTemp!=2147483647){
  ips.drawFastHLine(0, map(setTemp,rangeMin,rangeMax,79,0),160,0xfa00);
  ips.setCursor(0,map(setTemp,rangeMin,rangeMax,79,0)+2);
  ips.setTextColor(0xfa00);
  ips.print("Set:");
  ips.print(setTemp/10);
  ips.write('.');
  ips.print(setTemp%10);
  }
  ips.setTextColor(TFT_WHITE);
  ips.setCursor(0,0);
  ips.print(rangeMax/10);
  ips.write('.');
  ips.print(rangeMax%10);
  ips.setCursor(0,72);
  ips.print(rangeMin/10);
  ips.write('.');
  ips.print(rangeMin%10);
  for(int i = 1;i<160;i++){
    ips.drawLine(i-1,map(dat[(i-1+historyCursor)%160],rangeMin,rangeMax,79,0),
                   i,map(dat[(  i+historyCursor)%160],rangeMin,rangeMax,79,0),TFT_CYAN);
  }
  ips.setTextColor(TFT_ORANGE);
  ips.setCursor(130,vpos-4);
  ips.print(dat[(historyCursor+159)%160]/10);
  ips.write('.');
  ips.print(dat[(historyCursor+159)%160]%10);
}
void draw_graph_rpm(int *dat, int rangeMin, int rangeMax, int setRpm){
  ips.fillScreen(0x0000); //初始黑色
  ips.setCursor(30,0);
  for(int i = 1;i < 5;i++)
    ips.drawFastHLine(0,i*16,160,0x6300);//0x30c用于显示背景网格, 0x618用于显示设定值
  for(int i = 0;i < 10;i++)
    ips.drawFastVLine(i*16+15-(historyCursor%16),0,160,0x6300);//0x30c用于显示背景网格, 0x618用于显示设定值
  ips.setTextColor(TFT_DARKGREY);
  ips.print("Motor Speed Wave");
  int vpos = map(dat[(historyCursor+159)%160],rangeMin,rangeMax,79,0);
  if(vpos>76) vpos = 76;
  else if(vpos<4) vpos = 4;
  if(setRpm!=2147483647){
  ips.drawFastHLine(0, map(setRpm,rangeMin,rangeMax,79,0),160,0xfa00);
  ips.setCursor(0,map(setRpm,rangeMin,rangeMax,79,0)-8);
  ips.setTextColor(0xfa00);
  ips.print("Set:");
  ips.print(setRpm);
  }
  ips.setTextColor(TFT_WHITE);
  ips.setCursor(0,0);
  ips.print(rangeMax);
  ips.setCursor(0,72);
  ips.print(rangeMin);
  for(int i = 1;i<160;i++){
    ips.drawLine(i-1,map(dat[(i-1+historyCursor)%160],rangeMin,rangeMax,79,0),
                   i,map(dat[(  i+historyCursor)%160],rangeMin,rangeMax,79,0),TFT_YELLOW);
  }
  ips.setTextColor(TFT_PINK);
  ips.setCursor(130,vpos-4);
  ips.print(dat[(historyCursor+159)%160]);
}

uint8_t getBtn(){
  if(!digitalRead(KEY_OK)) return KEY_OK;
  if(!digitalRead(KEY_LEFT)) return KEY_LEFT;
  if(!digitalRead(KEY_RIGHT)) return KEY_RIGHT;
  if(!digitalRead(KEY_UP)) return KEY_UP;
  if(!digitalRead(KEY_DOWN)) return KEY_DOWN;
  return 0;
}
int getNum(int initial, const char *assigns){
  ips.fillScreen(TFT_DARKGREY);
  cn.setBackgroundColor(TFT_DARKGREEN);
  cn.setForegroundColor(TFT_CYAN);
  for(uint8_t i = 1; i <= 14;i++){
    ips.fillRect(0,0,160,i,TFT_DARKGREEN);
    cn.drawUTF8(2,i-2,assigns);
    ips.fillRect(48,58-i*3,64,64,0);
    ips.fillRect(49,58-i*3,62,15,TFT_NAVY);
    ips.drawRect(48,58-i*3,64,64,0xffff);
    ips.setTextColor(0xff00);
    ips.setCursor(52,62-i*3);
    ips.print(initial);
    delay(i*2);
  }
  while(!digitalRead(KEY_OK)); //等待释放按键
  delay(10); //去抖动
  int w = initial;
  bool sig = 0;
  bool disp = 1;
  uint8_t cursor = 0;
  if(w<0){
    w = -w;
    sig = 1;
  }
  ips.setTextColor(0xffff);
  static const char * ch_keyboard= "789<4560123>";
  for(uint8_t i = 0;i < 12; i++){
    ips.setCursor(52+(i%4)*16,36+(i/4)*16); //52,36
    if(i == 7 && w == 0) ips.write('-');
    else ips.write(ch_keyboard[i]);
  }
  while(1){
    uint8_t got = getBtn();
    if(got){
      digitalWrite(12,1);
      delay(10);
      digitalWrite(12,0);
    }
    if(got == KEY_LEFT) { //左移
      if(cursor%4) cursor--;
      else cursor +=3;
      disp=1;
    }
    else if(got == KEY_RIGHT) { //右移
      if(cursor%4 != 3) cursor++;
      else cursor -=3;
      disp=1;
    }
    else if(got == KEY_UP) { //上移
      if(cursor/4) cursor-=4;
      else cursor +=8;
      disp=1;
    }
    else if(got == KEY_DOWN) { //下移
      if(cursor/4 < 2) cursor+=4;
      else cursor -=8;
      disp=1;
    }
    else if(got == KEY_OK) {
      if(cursor == 11){
        cn.setBackgroundColor(TFT_BROWN);
        cn.setForegroundColor(TFT_YELLOW);
        for(uint8_t i = 0;i<16;i++){
          ips.fillRect(0,32,160,20,TFT_BROWN);
          cn.drawUTF8(160-5*i,48,"数据已更新");
          delay(i);
        }
        for(uint8_t i = 0;i<48;i++){
          ips.fillRect(0,32,160,20,TFT_BROWN);
          cn.drawUTF8(80-i,48,"数据已更新");
          delay(i);
        }
        cn.setBackgroundColor(0);
        ips.fillScreen(0);
        return sig?-w:w;
      }
      else if(cursor == 3){
        w/=10;
        if(!w){
        ips.setCursor(100,52);
        ips.setTextColor(0x0);
        ips.write('0');
        ips.setCursor(100,52);
        ips.setTextColor(0xffff);
        ips.write('-');
        }
      }
      else if(cursor == 7 && !w){
        sig = !sig;
      }
      else if(w<10000000){
        w *=10;
        w += ch_keyboard[cursor] - 48;
        ips.setCursor(100,52);
        ips.setTextColor(0x0);
        ips.write('-');
        ips.setCursor(100,52);
        ips.setTextColor(0xffff);
        ips.write('0');
      }
      disp = 1;
    }
    if(disp){
      while(getBtn());
      delay(10);
      for(uint8_t i = 0;i <12; i ++){
        ips.drawRect(50+(i%4)*16,34+(i/4)*16,12,12,i == cursor?0xffff:0);
      }
      ips.fillRect(49,17,62,14,TFT_NAVY);
      ips.setCursor(52,20);
      ips.setTextColor(0xff00);
      if(sig) ips.write('-');
      ips.print(w);
      disp = 0;
    }
    yield();
  }
  return rand()%10000;
}

int menuSelect(){
  uint8_t btnsel = 0;
  ips.fillTriangle(40,0,120,0,80,40,TFT_RED);
  cn.setBackgroundColor(TFT_RED);
  cn.setForegroundColor(TFT_WHITE);
  cn.drawUTF8(60,14,"正弦波");
  ips.fillTriangle(40,0,40,80,80,40,TFT_GREEN);
  cn.setBackgroundColor(TFT_GREEN);
  cn.setForegroundColor(TFT_BLACK);
  cn.drawUTF8(42,32,"锯");
  cn.drawUTF8(42,44,"齿");
  cn.drawUTF8(42,56,"波");
  ips.fillTriangle(120,80,40,80,80,40,TFT_BLUE);
  cn.setBackgroundColor(TFT_BLUE);
  cn.setForegroundColor(TFT_WHITE);
  cn.drawUTF8(60,76,"三角波");
  ips.fillTriangle(120,80,120,0,80,40,TFT_WHITE);
  cn.setBackgroundColor(TFT_WHITE);
  cn.setForegroundColor(TFT_BLACK);
  cn.drawUTF8(106,38,"方");
  cn.drawUTF8(106,50,"波");
  while((btnsel = getBtn())== 0) yield();
  while(getBtn()!= 0) yield();
  digitalWrite(12,1);
  delay(10); //去抖
  digitalWrite(12,0);
  delay(45);
  digitalWrite(12,1);
  delay(10);
  digitalWrite(12,0);
  cn.setBackgroundColor(TFT_BLACK);
  if(btnsel == KEY_UP) return 1;
  else if(btnsel == KEY_RIGHT) return 2;
  else if(btnsel == KEY_DOWN) return 3;
  else if(btnsel == KEY_LEFT) return 4;
  return 0;
}

void emergencyStop(int reason){
  if(reason>=2 && getCntPwm()<512 && getCntPwm()>-512) return;
  setHeat(0);
  setFanMotorSpeed(2147483647);
  ips.fillScreen(0xf800);
  cn.setBackgroundColor(0xf800);
  cn.setForegroundColor(0xffff);
  cn.setCursor(50,40);
  cn.print("紧急停止!");
  cn.setCursor(28,52);
  if(reason>=2){
    cn.print("原因: 电流达到");
    cn.print(reason/1000);
    cn.print("mA");
    cn.setBackgroundColor(0x0000);
    while(getBtn() != KEY_OK){
      digitalWrite(12,1);
      delay(450);
      digitalWrite(12,0);
      delay(50);
      if(getMicroAmp() < ampOverflow) break;
    }
    while(getBtn() == KEY_OK)
      delay(10);
  }
  else if(reason ==1){
    bool silence = 0;
    cn.setCursor(16,52);
    cn.print("原因: 温度超过最大温度");
    cn.setBackgroundColor(0x0000);
    while(getTemperature10x() >= 840){
      if(!silence){
        digitalWrite(12,1);
        delay(250);
        digitalWrite(12,0);
        delay(50);
      }
      if(getBtn() == KEY_OK) {
        silence = 1;
      }
    }
    while(getBtn() == KEY_OK)
      delay(10);
    bootMillis = millis();
  }
  else{
    while(getBtn() == KEY_OK) yield();
      delay(500);
    cn.print("原因: 用户请求停止");
    cn.setCursor(4,64);
    cn.print("向上滑动摇杆设置过流电流");
    cn.setBackgroundColor(0x0000);
    while(getBtn() != KEY_OK) {
      yield();
      if(getBtn() == KEY_UP){
        ampOverflow = getNum(ampOverflow/1000,"设置过流电流, 单位毫安")*1000;
        if(ampOverflow<10000){
          ips.fillRect(0,32,160,20,TFT_BROWN);
          cn.setBackgroundColor(TFT_BROWN);
          cn.drawUTF8(26,48,"设置的保护电流太小!");
          cn.setBackgroundColor(0x0000);
          while(!getBtn());
          delay(10);
          ampOverflow = 800000;
          return;
        }
      }
    }
    while(getBtn() == KEY_OK) yield();
      delay(10);
  }
  bootMillis = millis();
}

void showKeeperInfo(){
  int nowError = abs(temp10x - timTargetTemp);
  if(timMode != 2) {
    maxError = 0; //复位最大误差
    outTemp = 0; //复位超出范围的次数
    lastErr = 0; //复位上次的误差
  }
  else if(nowError>maxError) maxError = nowError;
  if(nowError > e && lastErr <= e){
    outTemp ++;
  }
  lastErr = nowError;
  if(showMap == 0){
  ips.setTextColor(TFT_LIGHTGREY);
  ips.setCursor(98,0);
  ips.print("Heat time:");
  ips.setCursor(98,8);
  if(timMode == 1) ips.print(millis() - timStartHeatMillis);
  else ips.print(timStartMillis - timStartHeatMillis);
  ips.setCursor(98,16);
  ips.print("Max Error:");
  ips.setCursor(98,24);
  ips.printf("%d.%d oC",maxError/10,maxError%10);
  ips.setCursor(98,32);
  ips.print("Out = ");
  ips.print(outTemp);
  ips.setCursor(98,40);
  ips.printf("e = %d.%d",e/10,e%10);
  }
  else{
    int valSet;
    ips.setTextColor(ips.color565(64,128,96));
    if(timMode == 2) valSet = time_set - (millis() - timStartMillis)/1000;
    else valSet = time_set;
    ips.setCursor(32,72);
    ips.printf("%d.%doC, %d sec.",timTargetTemp/10,timTargetTemp%10,valSet);
  }
}