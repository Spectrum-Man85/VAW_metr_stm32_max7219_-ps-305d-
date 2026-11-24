#ifndef TEMP_FAN_H
#define TEMP_FAN_H

extern float minTempHs;
extern float maxTempHs;
extern float minTempTr;
extern float maxTempTr;
extern float PWM_MIN_DUTYHS;
extern float PWM_MAX_DUTYHS;
extern float PWM_MIN_DUTYTR;
extern float PWM_MAX_DUTYTR;

void tempFanInit();
void tempFanTick();

#endif
