#pragma once
#ifndef __ESPUINO_MPR121_H__
#define __ESPUINO_MPR121_H__

void ButtonMPR121_Init(void);
void ButtonMPR121_Cyclic(void);

//############# MPR121-based Touch configuration ######################
#ifdef PORT_TOUCHMPR121_ENABLE
const int numElectrodes = 4;
//t_button touchbuttons[numElectrodes];    // next + prev + pplay + rotEnc + button4 + button5 + dummy-button

#endif
#endif