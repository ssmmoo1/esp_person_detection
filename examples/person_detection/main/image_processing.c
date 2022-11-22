#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"

#include "esp_camera.h"
#include "image_processing.h"

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240


static const char *TAG1 = "downscaling";
static const char *TAG2 = "grayscaling";

/* Set up gray scale method*/
void grayscale_task_setup() {
    return;
}

/* Set up pdetect task*/
void downscale_task_setup() {
    return;
}

/* Main method for downscaling */
void downscale_task_loop(QueueHandle_t input_q, QueueHandle_t output_q) {
    /* Read camera_fb */
    uint8_t* fb = NULL;
    xQueueReceive(input_q, &fb, portMAX_DELAY);

    /* Read gray_scale buffer */
    uint8_t* grayscale_img = NULL;
    xQueueReceive(input_q, &grayscale_img, portMAX_DELAY);

    /* Allocate downscale buffer pointer*/
    ESP_LOGI(TAG1, "Allocating downscaled image memory");
    uint8_t* downscaled_img = NULL;
    while(downscaled_img == NULL) {
        downscaled_img = malloc(96*96);
    }

    /* Perform downscaling */
    uint8_t row_pix = IMAGE_HEIGHT / 96;  // 480 -> 5
    uint8_t col_pix = IMAGE_WIDTH / 96;

    size_t width = IMAGE_WIDTH;
    size_t height = IMAGE_HEIGHT;

    uint32_t pixel_window_size = row_pix * col_pix;
    ESP_LOGI(TAG1, "Performing Downscale");
    for(int row = 0; row < 96; row ++) {
        uint32_t downscaled_row_index = 96 * row;
        for(int col = 0; col < 96; col++) {
            uint32_t row_start = row * row_pix;
            uint32_t row_end = (row * row_pix) + row_pix;
            uint32_t col_start = col * col_pix;
            uint32_t col_end = (col * col_pix) + col_pix;
            uint64_t sum = 0;

            for(int r = row_start; r < row_end; r ++) {
                uint32_t row_index = r * width;
                for(int c = col_start; c < col_end; c++) {
                    sum += grayscale_img[row_index + c];
                }
            }
            downscaled_img[downscaled_row_index + col] = sum / pixel_window_size;
        }
    }

    /* Free Gray_scale buffer*/
    free(grayscale_img);

    /* Pass Camera fb */
    ESP_LOGI(TAG1, "Downscale: Writing camera fb");
    xQueueSend(output_q, &fb, portMAX_DELAY);

    /* Pass downscale buffer to PD*/
    ESP_LOGI(TAG1, "Downscale: Writing downscaled");
    xQueueSend(output_q, &downscaled_img, portMAX_DELAY);

    /* Vtask Delay*/
    vTaskDelay(1);
}

/* Main task for downscaling*/
void grayscale_task_loop(QueueHandle_t input_q, QueueHandle_t output_q) {
    /* Read camera_fb from input_q*/
    uint8_t* fb = NULL;

    ESP_LOGI(TAG2, "Waiting for full image");
    xQueueReceive(input_q, &fb, portMAX_DELAY);

    /* Malloc grayscale buffer - try catch*/
    ESP_LOGI(TAG2, "Allocating grayscale memory");
    uint8_t* grayscale_img  = NULL;
    while(grayscale_img == NULL) {
        grayscale_img = malloc(IMAGE_HEIGHT * IMAGE_WIDTH);
    }

    /* Perform gray scale */
    int index = 0;
    for(int i = 0; i < IMAGE_HEIGHT*IMAGE_WIDTH; i++) {
        uint16_t pixel = (fb[index]);
        pixel = (pixel << 8) + fb[index+1];
        index +=2;

        uint8_t red =   (pixel & 0b1111100000000000) >> 11;
        uint8_t green = (pixel & 0b0000011111100000) >> 5;
        uint8_t blue =  (pixel & 0b0000000000011111);

        uint8_t gray = red + green + blue;

        grayscale_img[i] = (gray);
    }

    /* Write camera_fb*/
    ESP_LOGI(TAG2, "Grayscale: Writing camera fb");
    xQueueSend(output_q, &fb, portMAX_DELAY);

    /* Write gray_scale buffer*/
    ESP_LOGI(TAG2, "Grayscale: Writing grayscaled");
    xQueueSend(output_q, &grayscale_img, portMAX_DELAY);

    /* VTask Delay */
    vTaskDelay(1);
}