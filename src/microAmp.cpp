#include "microAmp.h"

void initMicroAmp(){
  pinMode(1,ANALOG);
  for(uint8_t i = 0;i < 10;i++){
    historyVal[i] = analogRead(1);
    delayMicroseconds(4);
  }
  hcur = 0;
}
int getMicroAmp(){
//3.56mA	17.5mA	52.8mA	100mA	192mA	326mA	478mA	                                -5mA	-19.7mA	-103mA	-200mA	-298mA	-488mA
//3578.092593	3591.913043	3618.753425	3642.032258	3675.974359	3715.6	3764.730337	3562.863636	3553	3531.730769	3511.833333	3491.767857	3471.723404
  const int valTable[] = {
    34717, 34917, 35118, 35317, 35530, 35628, 35781, 35919, 36187, 36420, 36759, 37156, 37647
  };
  const int ampTable[] = {
    -488000, -298000, -200000, -103000, -19700, -5000, 3560, 17500, 52800, 100000, 192000, 326000, 478000
  };
  int bkf = 13; //插值索引
  int sum = 0;  //总和(多次读取取平均)

  historyVal[hcur<<1] = analogRead(1);
  delayMicroseconds(4);
  historyVal[(hcur<<1)+1] = analogRead(1);
  if(hcur == 4) hcur = 0;
  else hcur++;

  for(uint8_t i = 0;i<10;i++){
    sum+=historyVal[i];
  }
  for(uint8_t i = 0;i<13;i++){
    if(sum<valTable[i]){
      bkf = i;
      break;
    }
  }
  if(bkf > 0 && bkf < 13){
    return map(sum,valTable[bkf-1], valTable[bkf],ampTable[bkf-1], ampTable[bkf]);
  }
  else if(bkf==13){
    return int(311.439f*sum-11247846.4f);
  }
  return -500000;
}