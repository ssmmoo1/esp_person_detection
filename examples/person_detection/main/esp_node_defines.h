
#ifndef ESP_NODE_DEFINES_H
#define ESP_NODE_DEFINES_H

#include "esp_camera.h"

#define R_640x480
// #define R_400x296
// #define R_640x480
// #define R_800x600
// #define R_1024x968

#ifdef R_320x240
#define IMAGE_HEIGHT 240
#define IMAGE_WIDTH 320
#define CAMERA_INPUT FRAMESIZE_QVGA
#endif

#ifdef R_400x296
#define IMAGE_HEIGHT 296
#define IMAGE_WIDTH 400
#define CAMERA_INPUT FRAMESIZE_CIF
#endif

#ifdef R_640x480
#define IMAGE_HEIGHT 480
#define IMAGE_WIDTH 640
#define CAMERA_INPUT FRAMESIZE_VGA
#endif

#ifdef R_800x600
#define IMAGE_HEIGHT 600
#define IMAGE_WIDTH 800
#define CAMERA_INPUT FRAMESIZE_SVGA
#endif

#ifdef R_1024x968
#define IMAGE_HEIGHT 768
#define IMAGE_WIDTH 1024
#define CAMERA_INPUT FRAMESIZE_XGA
#endif

#ifdef R_1280x1024
#define IMAGE_HEIGHT 1024
#define IMAGE_WIDTH 1280
#define CAMERA_INPUT FRAMESIZE_SXGA
#endif

#endif