/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_camera.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


#define HOST_IP_ADDR "192.168.0.246"
#define PORT 3333

static const char *TAG = "networking";
static const char *header = "IMAGE_HEADER";
static const int image_size = 96 * 96;


static void network_task_loop(QueueHandle_t input_q)
{
    int8_t* frame_buf;
    camera_fb_t* fb;
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", HOST_IP_ADDR, PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) 
        {

            xQueueReceive(input_q, &fb, portMAX_DELAY);
            frame_buf = (int8_t*) fb->buf;
            /*
            //Send image_header
            err = send(sock, header, strlen(header), 0);
            if(err < 0)
            {
                ESP_LOGE(TAG, "Error sending image header");
            }
            err = send(sock, (void *) (&image_size), sizeof(int), 0);
            if(err < 0)
            {
                ESP_LOGE(TAG, "Error sending image size");
            }
            */

           int bytes_per_send = 512;
           int i;
           for(i = 0; i < image_size; i+=bytes_per_send)
           {
            err = send(sock, frame_buf+i, bytes_per_send, 0);
            if(err < 0)
            {
                ESP_LOGE(TAG, "Error sending image buffer");
            }
           }
           err = send(sock, frame_buf+i-bytes_per_send, image_size - i, 0);
           if(err < 0)
           {
                ESP_LOGE(TAG, "Error sending image buffer");
           }

            free(frame_buf);

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);
            }
            /* Return the frame buffer */
            esp_camera_fb_return(fb);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}