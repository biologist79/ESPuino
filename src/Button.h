#pragma once

typedef struct {
	bool lastState : 1;
	bool currentState : 1;
	bool isPressed : 1;
	bool isReleased : 1;
	unsigned long lastPressedTimestamp;
	unsigned long lastReleasedTimestamp;
	unsigned long firstPressedTimestamp;
} t_button;

extern uint8_t gShutdownButton;
extern bool gButtonInitComplete;

void Button_Init(void);
void Button_Cyclic(void);
