#pragma once

extern QueueHandle_t gVolumeQueue;
extern QueueHandle_t gTrackQueue;
extern QueueHandle_t gTrackControlQueue;
extern QueueHandle_t gRfidCardQueue;
extern QueueHandle_t gEqualizerQueue;

void Queues_Init(void);
