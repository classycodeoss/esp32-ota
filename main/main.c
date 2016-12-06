//
//  main.c
//  esp32-ota
//
//  Main program for OTA demo application
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

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "network.h"
#include "ota_update.h"


// *****************************************************
// TODO - update these definitions for your environment!
// *****************************************************
#define AP_SSID             "accesspoint-name"
#define AP_PASSWORD         "accesspoint-password"


#define SOFTWARE_VERSION    "Factory-prepared"


#define TAG "main"


int app_main(void)
{
    ESP_LOGI(TAG, "intialization started");
    ESP_LOGI(TAG, "software version: %s", SOFTWARE_VERSION);
    
    nvs_flash_init();


    // Connect to an access point to receive OTA firmware updates.

    networkInit();
    networkConnect(AP_SSID, AP_PASSWORD);

    ESP_LOGI(TAG, "intialization completed");


    // This code assumes that there's an LED connected to GPIO 5 (sparkfun ESP32 Thing).

    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    while (1) {
    
        int nofFlashes = 1;
        if (networkIsConnected()) {
            nofFlashes += 1;
        }
        if (otaUpdateInProgress()) {
            nofFlashes += 2; // results in 3 (not connected) or 4 (connected) flashes
        }
        
        for (int i = 0; i < nofFlashes; i++) {
            gpio_set_level(GPIO_NUM_5, 1);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_NUM_5, 0);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // will never arrive here
    return 0;
}
