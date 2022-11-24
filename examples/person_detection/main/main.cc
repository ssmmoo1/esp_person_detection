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

#include "main_functions.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "networking.c"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "app_camera_esp.h"
#include "esp_camera.h"
#include "image_processing.h"
#include "esp_node_defines.h"

#include "esp_main.h"

#ifdef ENABLE_TIMING
  struct timeval g_tv_start[20];
  struct timeval g_tv_stop[20];
  uint8_t g_time_start_idx = 0;
  uint8_t g_time_stop_idx = 0;
#endif

#if CLI_ONLY_INFERENCE
#include "esp_cli.h"
#endif

//setup KPN queues for inputs to processes
static QueueHandle_t grayscale_q; //frame buffer pointer x1
static QueueHandle_t downscale_q; //frame buffer pointer x2
static QueueHandle_t pdetect_q; //frame buffer pointer x2
static QueueHandle_t network_q; //frame buffer pointer x1


//Take images with the camera
//No input queue
//Writes 1 token to output queue, pointer to image taken by camera 
void camera_task(void* args) {
#ifdef ENABLE_TIMING
  struct timeval tv_start;
  struct timeval tv_stop;
#endif

    //Setup camera 
    int ret = app_camera_init();
    if(ret != 0) {
      ESP_LOGE("Camera Task: ", "Camera init failed \n");
    }

    //Loop taking images 
    while(true) {
#ifdef ENABLE_TIMING
      gettimeofday(&tv_start, NULL);
      gettimeofday(&g_tv_start[g_time_start_idx], NULL);
      g_time_start_idx++;
#endif
      //Get image from camera
      camera_fb_t* fb = NULL;
      while(fb == NULL) {
        fb = esp_camera_fb_get();
      }
      ESP_LOGI("Camera Task: ", "Camera captured a frame\n");

      ESP_LOGI("Camera Task: ", "Trying to malloc buffer\n");
      uint8_t* frame_buf = NULL;
      while(frame_buf == NULL) {
        frame_buf = (uint8_t *) malloc(IMAGE_HEIGHT * IMAGE_WIDTH * 2);
        vTaskDelay(0);
      }

      memcpy(frame_buf, fb->buf, IMAGE_HEIGHT * IMAGE_WIDTH * 2);
      ESP_LOGI("Camera Task: ", "Copied image to new buffer\n");

      memcpy(frame_buf, fb->buf, IMAGE_WIDTH * IMAGE_HEIGHT * 2);
      esp_camera_fb_return(fb);

      ESP_LOGI("Camera Task: ", "Returned camera fb\n");
    
      ESP_LOGI("Camera Task: ", "Sending image to person detection");
      xQueueSend(grayscale_q, &frame_buf, portMAX_DELAY);
      ESP_LOGI("Camera Task: ", "Send image to person detection");
#ifdef ENABLE_TIMING
      gettimeofday(&tv_stop, NULL);
      float time_sec = tv_stop.tv_sec - tv_start.tv_sec + 1e-6f * (tv_stop.tv_usec - tv_start.tv_usec);
      printf("Camera Loop Time Taken: %f sec\n", time_sec);
#endif
      vTaskDelay(200 / portTICK_PERIOD_MS);
      //vTaskDelay(1);

    }

}

//Converts image to grayscale 
/* Outputs: 2 tokens to downscale queue: original and grayscal image */
void grayscale_task(void* args) {
  grayscale_task_setup();

#ifdef ENABLE_TIMING
  struct timeval tv_start;
  struct timeval tv_stop;
#endif

  while(true) {
#ifdef ENABLE_TIMING
    gettimeofday(&tv_start, NULL);
#endif

    grayscale_task_loop(grayscale_q, downscale_q);
    
    /* Vtask Delay*/
    vTaskDelay(1);
    //vTaskDelay(0);

#ifdef ENABLE_TIMING
    gettimeofday(&tv_stop, NULL);
    float time_sec = tv_stop.tv_sec - tv_start.tv_sec + 1e-6f * (tv_stop.tv_usec - tv_start.tv_usec);
    printf("Grayscale Loop Time Taken: %f sec\n", time_sec);
#endif
  }

}

//Downscale images to 96x96
//Reads 2 token from input queue: pointer to original image and grayscale image 
//Writes 2 token to output queue: pointer to original image and pointer to downscaled image 
void downscale_task(void* args) {
#ifdef ENABLE_TIMING
  struct timeval tv_start;
  struct timeval tv_stop;
#endif

    downscale_task_setup();

    while(true) {
#ifdef ENABLE_TIMING
    gettimeofday(&tv_start, NULL);
#endif

    downscale_task_loop(downscale_q, pdetect_q);
      
    /* VTask Delay */
    vTaskDelay(1);
    //vTaskDelay(0);

#ifdef ENABLE_TIMING
    gettimeofday(&tv_stop, NULL);
    float time_sec = tv_stop.tv_sec - tv_start.tv_sec + 1e-6f * (tv_stop.tv_usec - tv_start.tv_usec);
    printf("Downscale Loop Time Taken: %f sec\n", time_sec);
#endif
    }
}


//Tensorflow person detection task 
//Reads 2 tokens from input queue,  pointer to original image and pointer to 96x96 grayscale image
//Writes 1 token to output queue, pointer to original image if a person was detected
//Reads in 96x96 grayscale images and puts full resolution color image pointer into 
void pdetect_task(void* args) {
#ifdef ENABLE_TIMING
  struct timeval tv_start;
  struct timeval tv_stop;
#endif
  pdetect_task_setup();

  while (true) {
#ifdef ENABLE_TIMING
    gettimeofday(&tv_start, NULL);
#endif

    pdetect_task_loop(pdetect_q, network_q);

    /* VTask Delay */
    vTaskDelay(1);
    //vTaskDelay(0);
  
#ifdef ENABLE_TIMING
    gettimeofday(&tv_stop, NULL);
    gettimeofday(&g_tv_stop[g_time_stop_idx], NULL);
    float time_sec = tv_stop.tv_sec - tv_start.tv_sec + 1e-6f * (tv_stop.tv_usec - tv_start.tv_usec);
    printf("Person Detection Loop Time Taken: %f sec\n", time_sec);
    float time_sec2 = tv_stop.tv_sec - g_tv_start[g_time_stop_idx].tv_sec + 1e-6f * (tv_stop.tv_usec - g_tv_start[g_time_stop_idx].tv_usec);
    printf("Latency: %f sec\n", time_sec2);
    if(g_time_stop_idx >=1) {
      float throughput = g_tv_stop[g_time_stop_idx].tv_sec - g_tv_stop[g_time_stop_idx - 1].tv_sec + 1e-6f * (g_tv_stop[g_time_stop_idx].tv_usec - g_tv_start[g_time_stop_idx -1].tv_usec);
      printf("throughput: %f sec\n", throughput);
    }
    g_time_stop_idx++;
#endif
  }
}

void network_task(void* args)
{
    //network_task_setup();
    network_task_loop(network_q);
}




extern "C" void app_main() {
  //Initialize stuff for networking 
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(example_connect());

  //Queues will hold pointers to frame buffers that are stored in global memory/heap/psram 
  //frame bufferes will be freed by the last process that uses it. 
  grayscale_q = xQueueCreate(1, sizeof(uint8_t*));
  downscale_q = xQueueCreate(2, sizeof(uint8_t*));
  pdetect_q = xQueueCreate(2, sizeof(uint8_t*));
  network_q = xQueueCreate(1, sizeof(uint8_t*));

  //Creat all tasks
  xTaskCreate(camera_task, "camera_task", 4*1024, NULL, 5, NULL);
  xTaskCreate(grayscale_task, "grayscale_task", 3* 1024, NULL, 5, NULL);
  xTaskCreate(downscale_task, "downscale_task", 3* 1024, NULL, 5, NULL);
  xTaskCreate(pdetect_task, "pdetect_task", 3 * 1024, NULL, 5, NULL);
  xTaskCreate(network_task, "network_task", 3* 1024, NULL, 6, NULL);
  vTaskDelete(NULL);
}
