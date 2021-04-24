#pragma once

#if (HAL == 2)
    #include "AC101.h"

    extern AC101 ac;
#endif

void AC101_Init(void);
