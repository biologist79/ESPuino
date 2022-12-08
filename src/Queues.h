#pragma once

extern QueueHandle_t gVolumeQueue;
extern QueueHandle_t gTrackQueue;
extern QueueHandle_t gTrackControlQueue;
extern QueueHandle_t gRfidCardQueue;

void Queues_Init(void);
