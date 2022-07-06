#ifndef _microAmp_H_FILE
#define _microAmp_H_FILE
#include <Arduino.h>

static int historyVal[10];
static uint8_t hcur = 0;
void initMicroAmp();
int getMicroAmp(); //返回微安电流

#endif