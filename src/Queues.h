#pragma once

extern QueueHandle_t gRfidCardQueue;
extern QueueHandle_t gLedQueue;

void Queues_Init(void);
