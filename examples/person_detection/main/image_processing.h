#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"


void grayscale_task_setup();
void downscale_task_setup();
void downscale_task_loop(QueueHandle_t input_q, QueueHandle_t output_q);
void grayscale_task_loop(QueueHandle_t input_q, QueueHandle_t output_q);