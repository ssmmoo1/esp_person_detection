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
void camera_task(void* args)
{
    //Setup camera 
    int ret = app_camera_init();
    if(ret != 0)
    {
      ESP_LOGE("Camera Task: ", "Camera init failed \n");
    }

    //Loop taking images 
    while(true)
    {
      //Get image from camera
      camera_fb_t* fb = NULL;
      while(fb == NULL) {
        fb = esp_camera_fb_get();
      }
      ESP_LOGI("Camera Task: ", "Camera captured a frame\n");

      ESP_LOGI("Camera Task: ", "Trying to malloc buffer\n");
      uint8_t* frame_buf = NULL;
      while(frame_buf == NULL)
      {
        frame_buf = (uint8_t *) malloc(IMAGE_HEIGHT * IMAGE_WIDTH * 2);
      }
      ESP_LOGI("Camera Task: ", "Copied image to new buffer\n");

      memcpy(frame_buf, fb->buf, IMAGE_HEIGHT * IMAGE_WIDTH * 2);
      esp_camera_fb_return(fb);

      ESP_LOGI("Camera Task: ", "Returned camera fb\n");
    
      ESP_LOGI("Camera Task: ", "Sending image to person detection");
      xQueueSend(grayscale_q, &frame_buf, portMAX_DELAY);
      ESP_LOGI("Camera Task: ", "Send image to person detection");
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      
    }

}

//Converts image to grayscale 
/* Outputs: 2 tokens to downscale queue: original and grayscal image */
void grayscale_task(void* args)
{
  grayscale_task_setup();
  while(true)
  {
    grayscale_task_loop(grayscale_q, downscale_q);
  }

}

//Downscale images to 96x96
//Reads 2 token from input queue: pointer to original image and grayscale image 
//Writes 2 token to output queue: pointer to original image and pointer to downscaled image 
void downscale_task(void* args)
{
    downscale_task_setup();
    while(true)
    {
      downscale_task_loop(downscale_q, pdetect_q);
    }
}


//Tensorflow person detection task 
//Reads 2 tokens from input queue,  pointer to original image and pointer to 96x96 grayscale image
//Writes 1 token to output queue, pointer to original image if a person was detected
//Reads in 96x96 grayscale images and puts full resolution color image pointer into 
void pdetect_task(void* args) {
  pdetect_task_setup();

  //TODO: Free downscale buffer pointer
  //TODO: Return camera frame back to camera 
  while (true) {
    pdetect_task_loop(pdetect_q, network_q);
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
