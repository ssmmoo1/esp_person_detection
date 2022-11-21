
/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "app_camera_esp.h"
#include "esp_camera.h"
#include "model_settings.h"
#include "image_provider.h"
#include "esp_main.h"

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#define SAVE_IMAGE 0
#define MOUNT_POINT "/sdcard"

static const char* TAG = "app_camera";

static uint16_t *display_buf; // buffer to hold data to be sent to display

#if SAVE_IMAGE
static void init_sdcard() {
  esp_err_t ret = ESP_FAIL;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;

  const char mount_point[] = MOUNT_POINT;
  ESP_LOGI(TAG, "Initializing SD card");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  ESP_LOGI(TAG, "Mounting SD card...");
  gpio_set_pull_mode((gpio_num_t)15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode((gpio_num_t)2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
  gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
  gpio_set_pull_mode((gpio_num_t)12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
  gpio_set_pull_mode((gpio_num_t)13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

  ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "SD card mount successfully!");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
  }

  // Card has been initialized, print its properties
  // sdmmc_card_print_info(stdout, card);

}
#endif

// Get the camera module ready
TfLiteStatus InitCamera(tflite::ErrorReporter* error_reporter) {
#if SAVE_IMAGE
  init_sdcard();
#endif
#if CLI_ONLY_INFERENCE
  ESP_LOGI(TAG, "CLI_ONLY_INFERENCE enabled, skipping camera init");
  return kTfLiteOk;
#endif
// if display support is present, initialise display buf
#if DISPLAY_SUPPORT
  if (display_buf == NULL) {
    display_buf = (uint16_t *) heap_caps_malloc(96 * 2 * 96 * 2 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (display_buf == NULL) {
    ESP_LOGE(TAG, "Couldn't allocate display buffer");
    return kTfLiteError;
  }
#endif

  int ret = app_camera_init();
  if (ret != 0) {
    TF_LITE_REPORT_ERROR(error_reporter, "Camera init failed\n");
    return kTfLiteError;
  }
  TF_LITE_REPORT_ERROR(error_reporter, "Camera Initialized\n");
  return kTfLiteOk;
}

/* Stores downscaled image */
typedef struct  {
    uint8_t* grayscale;
    //uint8_t* downscaled;
    size_t len;
    size_t width;
    size_t height;
} processed_img_t;

/* Convert to gray scale from original image*/
void grayscale(camera_fb_t* pic, processed_img_t* downscaled_pic) {
    //downscaled_pic->grayscale = heap_caps_malloc(pic->len / 2, MALLOC_CAP_SPIRAM);
    printf("Grayscaling\n");
    int index = 0;
    for(int i = 0; i < pic->len / 2; i++) {
        uint16_t pixel = (pic->buf[index]);
        pixel = (pixel << 8) + pic->buf[index+1];
        index +=2;

        uint8_t red =   (pixel & 0b1111100000000000) >> 11;
        uint8_t green = (pixel & 0b0000011111100000) >> 5;
        uint8_t blue =  (pixel & 0b0000000000011111);

        uint8_t gray = red + green + blue;

        downscaled_pic->grayscale[i] = (gray);
    }
    downscaled_pic->width = pic->width;
    downscaled_pic->height = pic->height;
    downscaled_pic->len = pic->width * pic->height;
}

/* Downscale after gray scale image */
void downscale_post_grayscale(processed_img_t* downscaled_img, int8_t* ret_buffer) {
    /* allocate space for 96 x 96 byte image*/
    printf("Downscale \n");
    uint8_t row_pix = downscaled_img->height / 96;  // 480 -> 5
    uint8_t col_pix = downscaled_img->width / 96;

    size_t width = downscaled_img->width;
    size_t height = downscaled_img->height;

    uint32_t pixel_window_size = row_pix * col_pix;

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
                    sum += downscaled_img->grayscale[row_index + c];
                }
            }
            ret_buffer[downscaled_row_index + col] = sum / pixel_window_size;
        }
    }
}

#if SAVE_IMAGE
uint64_t counterbmp2 =0;
void save_PGM_file_downscaled (int8_t* downscaled_img) {
    printf("Starting PGM for downscaled\n");
    const int dimx = 96, dimy =96;

    /* Opening file */
    char *pic_name = (char*) malloc(30 + sizeof(int64_t));
    printf("Saving to file1\n");
    sprintf(pic_name, MOUNT_POINT"/dow%lli.pgm", counterbmp2);
    counterbmp2++;
    FILE *fp = fopen(pic_name, "wb"); /* b - binary mode */

    /* Writing Header*/
    (void) fprintf(fp, "P2\n%d %d\n150\n", dimx, dimy);
    printf("Saving to file2\n");
    /* Writing to file pixel by pixel*/
    int index = 0;
    for (int j = 0; j < dimy; ++j) {
        for (int i = 0; i < dimx; ++i) {
            (void) fprintf(fp, "%d ", downscaled_img[index]);
            index++;
        }
        (void) fprintf(fp, "\n ");
    }
    //fwrite(downscaled_img->grayscale, 1, downscaled_img->len, fp);
    printf("Calling Free\n");
    (void) fclose(fp);
    free(pic_name);
    return;
}
#endif

void process_image(camera_fb_t* fb, int8_t* return_img) {
  processed_img_t processed_img;
  processed_img.grayscale = (uint8_t*) malloc(fb->width * fb->height);
  /* Grayscale image */
  grayscale(fb, &processed_img);
  /* Downscale image */
  downscale_post_grayscale(&processed_img, return_img);
  /* Free grayscale buffer */
  free(processed_img.grayscale);
  
#if SAVE_IMAGE
  save_PGM_file_downscaled(return_img);
#endif
}

void *image_provider_get_display_buf()
{
  return (void *) display_buf;
}

// Get an image from the camera module
TfLiteStatus GetImage(tflite::ErrorReporter* error_reporter, int image_width,
                      int image_height, int channels, int8_t* image_data) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGE(TAG, "Camera capture failed");
    return kTfLiteError;
  }

#if DISPLAY_SUPPORT
  // In case if display support is enabled, we initialise camera in rgb mode
  // Hence, we need to convert this data to grayscale to send it to tf model
  // For display we extra-polate the data to 192X192
  for (int i = 0; i < kNumRows; i++) {
    for (int j = 0; j < kNumCols; j++) {
      uint16_t pixel = ((uint16_t *) (fb->buf))[i * kNumCols + j];

      // for inference
      uint8_t hb = pixel & 0xFF;
      uint8_t lb = pixel >> 8;
      uint8_t r = (lb & 0x1F) << 3;
      uint8_t g = ((hb & 0x07) << 5) | ((lb & 0xE0) >> 3);
      uint8_t b = (hb & 0xF8);

      /**
       * Gamma corected rgb to greyscale formula: Y = 0.299R + 0.587G + 0.114B
       * for effiency we use some tricks on this + quantize to [-128, 127]
       */
      int8_t grey_pixel = ((305 * r + 600 * g + 119 * b) >> 10) - 128;

      image_data[i * kNumCols + j] = grey_pixel;

      // to display
      display_buf[2 * i * kNumCols * 2 + 2 * j] = pixel;
      display_buf[2 * i * kNumCols * 2 + 2 * j + 1] = pixel;
      display_buf[(2 * i + 1) * kNumCols * 2 + 2 * j] = pixel;
      display_buf[(2 * i + 1) * kNumCols * 2 + 2 * j + 1] = pixel;
    }
  }
#else
  TF_LITE_REPORT_ERROR(error_reporter, "Image Captured\n");

  /* Pre process image into grayscale */
  process_image(fb, image_data);
  TF_LITE_REPORT_ERROR(error_reporter, "Processing Completed\n");

  // We have initialised camera to grayscale
  // Just quantize to int8_t
  // for (int i = 0; i < image_width * image_height; i++) {
  //   image_data[i] = ((uint8_t *) fb->buf)[i] ^ 0x80;
  // }
#endif

  esp_camera_fb_return(fb);
  /* here the esp camera can give you grayscale image directly */
  return kTfLiteOk;
}
