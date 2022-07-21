#include "fanMotor.h"

void trigger_fun();
int getAvgRpm();

void fanMotorInit(){
  //PWM通道0 频率8KHz, 精度 8bit(0-255)
  //将GPIO0绑定到PWM通道1
  pinMode(13,OUTPUT); //电机方向控制
  pinMode(18,OUTPUT); //电机
  pinMode(19,OUTPUT); //加热
  ledcAttachPin(18,0); //电机
  ledcAttachPin(19,1); //加热
  ledcSetup(0,8000,12); //电机
  ledcSetup(1,8000,12); //加热
  ledcWrite(0,350);//电机关闭
  ledcWrite(1,0);//加热关闭
  digitalWrite(13,LOW); //电机方向控制:关闭
  for(ptr = 0; ptr <AVG_NUM;ptr++) avgRpm[ptr] = 2147483647/AVG_NUM; //初始化数组
  ptr = 0;
  microtrig = 0;
  pinMode(0,INPUT);
  attachInterrupt(0,trigger_fun,FALLING);
  
}
void setFanMotorSpeed(int s_peed){
  if(s_peed == 2147483647){
    ledcWrite(0,0);      //电机打开
    digitalWrite(13,LOW);
    return;
  }
  if(s_peed < 0){
    if(s_peed < -4095) s_peed = -4095;
    if(4095+s_peed > 4095-345) ledcWrite(0,4095-345);      //电机打开
    else ledcWrite(0,4095+s_peed);      //电机打开
    digitalWrite(13,HIGH);
  }
  else{
    if(s_peed > 4095) s_peed = 4095;
    if(s_peed < 345) ledcWrite(0,345);      //电机打开
    else ledcWrite(0,s_peed);      //电机打开
    digitalWrite(13,LOW);
  }
  //ledcWrite(1,0);//加热关闭, 因为电机和加热不同时进行
}

void setHeat(uint16_t s_heat){
  //if(s_heat) setFanMotorSpeed(0);//加热关闭, 因为电机和加热不同时进行
  ledcWrite(1,s_heat);      //电机打开
}

/**
 * @brief Get 次最近数据平均值
 * 风扇一圈为7个数据样本 7个样本的时间总和为一圈的时间, 单位微妙
 * 一分钟为60,000,000微秒
 * 60,000,000 / 7个样本
 * @return int 
 */
int getAvgRpm(){
  static uint32_t lastMicroTrig = 0;
  int res = 0;
  if(micros() - microtrig > 100000){
    int asum = 0;
    for(int i = 0; i <AVG_NUM;i++) asum += avgRpm[i];
    asum /= (AVG_NUM/11) ;
#if defined(WIFI_DEBUG_PORT_FANMOTOR)
    //client.printf("asum: %d, microtrig: %d, lastMicroTrig: %d\n",asum,microtrig,lastMicroTrig);
#endif
    if(asum>850 && microtrig != lastMicroTrig) {
      microtrig = micros();
      lastMicroTrig = microtrig; //此处lastMicroTrig, microtrig和micros的返回值是相同的
      return digitalRead(13)?-60000000/asum:60000000/asum;
    }
    detachInterrupt(0);
    for(ptr = 0; ptr <AVG_NUM;ptr++) avgRpm[ptr] = 2147483647/AVG_NUM; //初始化数组
    ptr = 0;
    microtrig = micros();
    attachInterrupt(0,trigger_fun,FALLING);
    return 0;
  }
  for(int i = 0; i < AVG_NUM ;i++) //初始化数组
    res += avgRpm[i];
  res /= (AVG_NUM/11) ;
  if(res) return digitalRead(13)?-60000000/res:60000000/res;
  return 0;
}
void trigger_fun(){
  uint32_t now_n = micros();
  if(now_n - microtrig >= 500){
    avgRpm[ptr] = now_n - microtrig;
    microtrig = now_n;
    if(ptr < AVG_NUM-1) ptr++;
    else ptr = 0;
  }
}


int getCntPwm(){
  if(digitalRead(13) == HIGH) return -4095 + ledcRead(0);
  return ledcRead(0);
}
int heat_keep(int _temp10x){ //维持在此温度需要的加热量, 输入温度, 输出加热强度
  if(_temp10x<250) return 200; //假定室温25摄氏度
  if(_temp10x>850) return 1400;
  return _temp10x*2-300;
}
// 温控核心代码
void THandleTempCtrl(int temp10x_set){
  //getTemperature10x();
  //setFanMotorSpeed(4095);
  //setHeat(200); //开启升温
  //constexpr  float dt = 1.0f; //控制倍率
  constexpr int extraHeat = 1;
  int16_t s;
  static float kp = 115.0;     //控制倍率 1000---0.04
  static float kd =  55.0;     //控制倍率 1000---0.04
  static float last_read;     //上次读取
  static float last_err;      //上次误差
  float err;                  //本次误差
  float _d;                   //微分控制
  int tempNow = getTemperature10x();

  err = temp10x_set - tempNow; //设定数值 - 读取数值
  if(temp10x_set == 0){
    setFanMotorSpeed(1); //关闭...
    setHeat(heat_keep(temp10x_set));
    return;
  }
  float pout = kp*err;
  float dout = (err - last_err)*kd;

  int outTotal = pout+dout;
  
  if(outTotal>=0){ //需要加热
    if(outTotal> 4095) outTotal = 4095; //最大散热值
    outTotal = (16769025-(4095 - outTotal)*(4095 - outTotal))/4095; //16769025为4095的平方
    outTotal +=heat_keep(temp10x_set);
    if(outTotal> 4095) outTotal = 4095; //最大散热值
    setFanMotorSpeed(355);
    setHeat(outTotal);
  }
  else{ //需要散热
  /*
    int tdelta = (1000-temp10x_set)/2;
    if(outTotal<tdelta-4095) outTotal =tdelta-3900;
    s = (heat_keep(temp10x_set)+outTotal) * extraHeat;
    outTotal = -((4095-tdelta)*(4095-tdelta)-(4095-tdelta + outTotal)*(4095-tdelta + outTotal))/(4095-tdelta); //15210000为3900的平方
    outTotal-=tdelta;
    if(outTotal<-4095) outTotal =-4095;
    setFanMotorSpeed(-outTotal);
    if(s<0) setHeat(0);
    else setHeat(s);
  */
    if(outTotal<-3800) outTotal =-3800;
    s = (heat_keep(temp10x_set)+outTotal/2) * extraHeat;
    outTotal = -(3800*3800-(3800 + outTotal)*(3800 + outTotal))/3800; //15210000为3900的平方
    outTotal-=295;
    setFanMotorSpeed(-outTotal);
    if(s<0)
    setHeat(0);
    else setHeat(s);
  }
#if defined(WIFI_DEBUG_PORT_FANMOTOR)
  if(client){
    client.printf("TempCtrl:Read: %4d, Set: %4d, Write: %4d, err: %7.2f, pout: %9.2f, dout: %9.2f, s: %3d\n",
      tempNow, temp10x_set, outTotal, err, pout, dout, s);
  if(client.available()){
    delay(10);
    char cmd = client.peek();
    if(cmd == 'p') {
      client.read();
      kp = client.parseFloat();
      client.print("updated KP: ");
      client.println(kp);
    }
    else if(cmd == 'd') {
      client.read();
      kd = client.parseFloat();
      client.print("updated KD: ");
      client.println(kd);
    }
    else if(cmd == 's') {
      client.read();
      ::temp10x_set = client.parseInt();
      temp10x_set = ::temp10x_set;
      client.print("updated Setting val: ");
      client.println(temp10x_set);
    }
    //else client.readString();
  }
  }
#endif

  last_err = err;
  last_read = tempNow;
}
void THandleSpeedCtrl(int rpm_set){
  //constexpr  float dt = 1.0f; //控制倍率
  static float kpx = 0.00008f;
  static float kdx = 0.00032f;
  static float last_read;     //上次读取
  static float last_err;      //上次误差
  float err;                  //本次误差
  float _d;                   //微分控制
  int rpmNow = getAvgRpm();
  float kp, kd;

  bool rpm_sign = 0; //方向
  if(rpm_set<0){
    rpm_sign = 1;
    rpm_set = -rpm_set; //形参
    rpmNow = -rpmNow;
  }
  if(rpm_set <= 1262) {
    float rpm_setf=0.985f*rpm_set+19;
    kp = kpx*rpm_setf;     //控制倍率 1000---0.04
    kd = kdx*rpm_setf;     //控制倍率 1000---0.04
    rpm_set = rpm_setf;
  }
  else{
    kp = kpx*rpm_set;     //控制倍率 1000---0.04
    kd = kdx*rpm_set;     //控制倍率 1000---0.04
  }
  if(rpmNow <= 10){
    kp*=8;
    kd*=8;
  }
  else if(last_read <= 10){
    kp = 0;
    kd = 0;
  }

  err = rpm_set - rpmNow; //设定数值 - 读取数值
  if(rpm_set == 0){
    setFanMotorSpeed(rpm_sign?-1:1); //关闭...
    return;
  }
  float pout = kp*err;
  float dout = (err - last_err)*kd;

  int outTotal;
  if(rpm_sign) outTotal = pout+dout-getCntPwm();
  else outTotal = pout+dout+getCntPwm();
  if(outTotal>4095) outTotal = 4095;
  if(outTotal<   1) outTotal =    1;
#if defined(WIFI_DEBUG_PORT_FANMOTOR)
  if(client){
  //client.printf("SpeedCtrl:Read: %4d , Set: %4d , Write: %4d , err: %7.2f , pout: %7.2f , dout: %7.2f\n",
  //  rpmNow, rpm_set, outTotal, err, pout, dout);
  if(client.available()){
    delay(10);
    char cmd = client.peek();
    if(cmd == 'p') {
      client.read();
      kpx = client.parseFloat();
      kp = kpx*rpm_set;     //控制倍率 1000---0.04
      client.print("updated KP: ");
      client.print(kp,5);
      client.print(", KPX: ");
      client.println(kpx,5);
    }
    else if(cmd == 'd') {
      client.read();
      kdx = client.parseFloat();
      kd = kdx*rpm_set;     //控制倍率 1000---0.04
      client.print("updated KD: ");
      client.print(kd,5);
      client.print(", KDX: ");
      client.println(kdx,5);
    }
    else if(cmd == 's') {
      client.read();
      ::rpm_set = client.parseInt();
      rpm_set = ::rpm_set;
      client.print("updated Setting val: ");
      client.println(rpm_set);
    }
    else client.readString();
  }
  }
#endif
  setFanMotorSpeed(rpm_sign?-outTotal:outTotal);

  last_err = err;
  last_read = rpmNow;
}

// temperature 温度----------------------------------------------

bool initDs18b20(){
  initial = true; //false 为正常, true为未检测到
  resetDs18b20();
  return (initial = hashDs18b20());
}
void resetDs18b20(){
  pinMode(DS18B20_PORT,OUTPUT);	//拉低DQ
  digitalWrite(DS18B20_PORT,LOW);
	delayMicroseconds(750);
  digitalWrite(DS18B20_PORT,HIGH);
	delayMicroseconds(20);	//20US
  pinMode(DS18B20_PORT,INPUT_PULLUP);	//拉低DQ
}

uint8_t hashDs18b20(){
  uint8_t time_temp=0;
  while(digitalRead(DS18B20_PORT) == 1&&time_temp<200)	//等待DQ为低电平
	{
		time_temp++;
		delayMicroseconds(1);
	}
	if(time_temp>=200) {
#if defined(WIFI_DEBUG_PORT_TEMP)
    client.println("Time out failed: 200 microsec.");
#endif
    return 1;	//如果超时则强制返回1
  }
	else time_temp=0;
	while((digitalRead(DS18B20_PORT) == 0)&&time_temp<200)	//等待DQ为高电平
	{
		time_temp++;
		delayMicroseconds(1);
	}
	if(time_temp>=200) {
#if defined(WIFI_DEBUG_PORT_TEMP)
    client.println("Hash failed: no ACK received/checksum not correct.");
#endif
    return 1;	//如果超时则强制返回1
  }
	return 0;
}

uint16_t getRaw(){
	uint8_t dath=0;
	uint8_t datl=0;

  initDs18b20();
	ds18b20_write_byte(0xcc);//SKIP ROM
  ds18b20_write_byte(0x44);//转换命令	
  initDs18b20();
	ds18b20_write_byte(0xcc);//SKIP ROM
  ds18b20_write_byte(0xbe);//读存储器

	datl=ds18b20_read_byte();//低字节
	dath=ds18b20_read_byte();//高字节
	return ((uint16_t)dath<<8)+datl;//合并为16位数据
}

int getTemperature10x(){
  static int lasttarget_v = 0x7fffffff;
  static uint32_t lastRead = 0;
  int target_v = 1;
  uint16_t raw = getRaw();
  uint8_t div1,div2;
  if(raw > 16384) { //零下
    target_v = -1;
    raw = ~raw; // 取反
  }
  div1 = raw&15; //小数部分
  div2 = raw>>4; //整数部分
  target_v = div2*10*target_v;
  target_v += div1*5/8;
  if(target_v>850) {
#if defined(WIFI_DEBUG_PORT_TEMP)
    client.print("target_v too high! Original: ");
    client.println(target_v);
#endif
    lastRead = millis();
  }
  else if(target_v<200) {
#if defined(WIFI_DEBUG_PORT_TEMP)
    client.print("target_v too low! Original: ");
    client.println(target_v);
#endif
    lastRead = millis();
  }
  if(millis() - lastRead > 1000 || (target_v - lasttarget_v <10 && target_v - lasttarget_v >-40)){
    lastRead = millis();
    lasttarget_v = target_v;
    return target_v;
  }
#if defined(WIFI_DEBUG_PORT_TEMP)
  client.print("target_v NOT in range (rising or dropping too fast): ");
  client.println(target_v - lasttarget_v);
#endif
  if(lasttarget_v == 0x7fffffff) return 851;
  return lasttarget_v;
}
uint8_t ds18b20_read_byte(void)
{
	uint8_t i=0;
	uint8_t dat=0;
	uint8_t temp=0;
  if(initial) return 0;
	for(i=0;i<8;i++)//循环8次，每次读取一位，且先读低位再读高位
	{
    {
      pinMode(DS18B20_PORT, OUTPUT);
      digitalWrite(DS18B20_PORT,LOW);
			delayMicroseconds(2);
      digitalWrite(DS18B20_PORT,HIGH);
			delayMicroseconds(2);
      pinMode(DS18B20_PORT, INPUT_PULLUP);
			delayMicroseconds(8);
	    temp = digitalRead(DS18B20_PORT);	//如果总线上为1则数据dat为1，否则为0
	    delayMicroseconds(50);
    }
		dat=(temp<<7)|(dat>>1);
	}
	return dat;	
}

void ds18b20_write_byte(uint8_t dat)
{
	uint8_t i=0;
	uint8_t temp=0;
  pinMode(DS18B20_PORT,OUTPUT);
	for(i=0;i<8;i++)//循环8次，每次写一位，且先写低位再写高位
	{
		temp=dat&0x01;//选择低位准备写入
		dat>>=1;//将次高位移到低位
		if(temp)
		{
      digitalWrite(DS18B20_PORT,LOW);
			delayMicroseconds(4);
      digitalWrite(DS18B20_PORT,HIGH);
			delayMicroseconds(60);
		}
		else
		{
      digitalWrite(DS18B20_PORT,LOW);
			delayMicroseconds(60);
      digitalWrite(DS18B20_PORT,HIGH);
			delayMicroseconds(4);
		}	
	}
  pinMode(DS18B20_PORT,INPUT_PULLUP);
}