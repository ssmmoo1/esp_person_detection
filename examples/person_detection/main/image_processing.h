#include "freertos/FreeRTOS.h"

#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

void grayscale_task_setup();
void downscale_task_setup();
void downscale_task_loop(QueueHandle_t input_q, QueueHandle_t output_q);
void grayscale_task_loop(QueueHandle_t input_q, QueueHandle_t output_q);

#ifdef __cplusplus
}
#endif

#endif