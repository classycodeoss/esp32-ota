//
//  network.c
//  esp32-ota
//
//  Networking code for OTA demo application
//
//  Created by Andreas Schweizer on 02.12.2016.
//  Copyright © 2016 Classy Code GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "network.h"
#include "ota_update.h"


#define TAG "network"

#define NORM_C(c) (((c) >= 32 && (c) < 127) ? (c) : '.')

#define AP_CONNECTION_ESTABLISHED (1 << 0)
static EventGroupHandle_t sEventGroup;


// Indicates that we should trigger a re-boot after sending the response.
static int sRebootAfterReply;


// Listen to TCP requests on port 80
static void networkTask(void *pvParameters);


static void processMessage(const char *message, char *responseBuf, int responseBufLen);
static void networkSetConnected(uint8_t c);
static esp_err_t eventHandler(void *ctx, system_event_t *event);


void networkInit()
{
    ESP_LOGI(TAG, "networkInit");
    
    ESP_ERROR_CHECK( esp_event_loop_init(eventHandler, NULL) );
    tcpip_adapter_init();

    sEventGroup = xEventGroupCreate();
    xTaskCreate(&networkTask, "networkTask", 32768, NULL, 5, NULL);
    
    otaUpdateInit();
}

void networkConnect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "networkConnect %s", ssid);
    
    wifi_init_config_t wlanInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&wlanInitConfig) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wlanStaConfig;
    strncpy(wlanStaConfig.sta.ssid, ssid, sizeof(wlanStaConfig.sta.ssid) / sizeof(char));
    strncpy(wlanStaConfig.sta.password, password, sizeof(wlanStaConfig.sta.password) / sizeof(char));
    wlanStaConfig.sta.bssid_set = false;
    
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wlanStaConfig) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

int networkIsConnected()
{
    return xEventGroupGetBits(sEventGroup) & AP_CONNECTION_ESTABLISHED;
}


static void networkTask(void *pvParameters)
{
    int e;
    
    ESP_LOGI(TAG, "networkTask");
    
    while (1) {

        // Barrier for the connection (we need to be connected to an AP).
        xEventGroupWaitBits(sEventGroup, AP_CONNECTION_ESTABLISHED, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "connected to access point");
        
        
        // Create TCP socket.
        
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            ESP_LOGE(TAG, "socket() failed");
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
        
        
        // Bind socket to port 80.
        
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof (struct sockaddr_in));
        serverAddr.sin_len = sizeof(struct sockaddr_in);
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(80);
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        
        ESP_LOGI(TAG, "bind(%d, ...)", s);
        int b = bind(s, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
        if (b < 0) {
            e = errno;
            ESP_LOGE(TAG, "bind() failed (%d, errno = %d)", b, e);
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
        
        
        // Listen to incoming connections.
        
        ESP_LOGI(TAG, "listen()");
        listen(s, 1); // backlog max. 1 connection
        while (1) {
            
            
            // Accept the connection on a separate socket.
            
            ESP_LOGI(TAG, "accept()");
            struct sockaddr_in clientAddr;
            socklen_t clen = sizeof(clientAddr);
            int s2 = accept(s, (struct sockaddr *)&clientAddr, &clen);
            if (s2 < 0) {
                e = errno;
                ESP_LOGE(TAG, "accept() failed (%d, errno = %d)", s2, e);
                vTaskDelay(1000 / portTICK_RATE_MS);
                break;
            }
            
            // (Would normally fork here)
            
            do {
                ESP_LOGI(TAG, "receiving data...");


                // Allocate and clear memory for the request data.
                
                int maxRequestLen = 10000;
                char *requestBuf = malloc(maxRequestLen * sizeof(char));
                if (!requestBuf) {
                    e = errno;
                    ESP_LOGE(TAG, "malloc for requestBuf failed (errno = %d)!", e);
                    break;
                }
                bzero(requestBuf, sizeof(requestBuf) / sizeof(char));
                
                
                // Read the request and store it in the allocated buffer.
                
                int totalRequestLen = 0;
                int n = 0;
                int e = 0;
                
                do {
                    ESP_LOGI(TAG, "read(%d, ..., %d)", s2, maxRequestLen - totalRequestLen);
                    n = read(s2, &requestBuf[totalRequestLen], maxRequestLen - totalRequestLen);
                    e = errno;
                    if (n > 0) {
                        totalRequestLen += n;
                    }
                } while (n > 0 && totalRequestLen < maxRequestLen);

                if (n < 0) {
                    free(requestBuf);
                    close(s2);
                    ESP_LOGE(TAG, "read() failed (%d)", e);
                    vTaskDelay(1000 / portTICK_RATE_MS);
                    break;
                }
            
            
                // Read completed successfully.
                // Process the request and create the response.
            
                ESP_LOGI(TAG, "received %d bytes: %02x %02x %02x %02x ... | %c%c%c%c...",
                         totalRequestLen,
                         requestBuf[0], requestBuf[1], requestBuf[2], requestBuf[3],
                         NORM_C(requestBuf[0]), NORM_C(requestBuf[1]), NORM_C(requestBuf[2]), NORM_C(requestBuf[3]));
            
            
                char responseBuf[1000];
                processMessage(requestBuf, responseBuf, sizeof(responseBuf) / sizeof(char));

                free(requestBuf);
                
                
                // Send the response back to the client.
            
                ESP_LOGI(TAG, "write() %d bytes", strlen(responseBuf));
                n = write(s2, responseBuf, strlen(responseBuf));
                if (n < 0) {
                    ESP_LOGE(TAG, "write() failed (%d)", n);
                } else {
                    ESP_LOGI(TAG, "write() succeeded, n = %d", n);
                }
                
            } while (0);
            
            ESP_LOGI(TAG, "closing socket %d", s2);
            close(s2);
            
            if (sRebootAfterReply) {
                ESP_LOGI(TAG, "Reboot in 2 seconds...");
                vTaskDelay(2000 / portTICK_RATE_MS);
                esp_restart();
            }
        }
        
        // Should never arrive here
        close(s);
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

static void processMessage(const char *message, char *responseBuf, int responseBufLen)
{
    // Response to send back to the TCP client.
    char response[256];
    sprintf(response, "OK\r\n");
    
    if (message[0] == '!') {
        TOtaResult result = OTA_OK;
        
        if (message[1] == '[') {
            result = otaUpdateBegin();
            
        } else if (message[1] == ']') {
            result = otaUpdateEnd();
            
        } else if (message[1] == '*') {
            sRebootAfterReply = 1;
            
        } else {
            result = otaUpdateWriteHexData(&message[1]);
        }

        if (result != OTA_OK) {
            sprintf(response, "OTA_ERROR %d\r\n", result);
        }
        
    } else if (message[0] == '?') {
        otaDumpInformation();
    }
    
    strncpy(responseBuf, response, responseBufLen);
}

static void networkSetConnected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    } else {
        xEventGroupClearBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    }
}

static esp_err_t eventHandler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
            
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_STA_START");
            esp_wifi_connect();
            break;
            
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_STA_GOT_IP");
            networkSetConnected(1);
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_STA_DISCONNECTED");
            // try to re-connect
            esp_wifi_connect();
            networkSetConnected(0);
            break;
        
        case SYSTEM_EVENT_AP_START:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_START");
            break;
        
        case SYSTEM_EVENT_AP_STOP:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STADISCONNECTED");
            break;
            
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STACONNECTED: " MACSTR " id=%d",
                     MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
            break;
        
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STADISCONNECTED: " MACSTR " id=%d",
                     MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
            break;
        
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STADISCONNECTED: " MACSTR " rssi=%d",
                     MAC2STR(event->event_info.ap_probereqrecved.mac), event->event_info.ap_probereqrecved.rssi);
            break;
            
        default:
            break;
    }
    
    return ESP_OK;
}
