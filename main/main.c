//#include <stdio.h>
//#include <stdbool.h>
//#include <unistd.h>

// #include "freertos/FreeRTOS.h"
// #include "esp_wifi.h"
// #include "esp_system.h"
// #include "esp_event.h"
// //#include "esp_event_loop.h"
// #include "nvs_flash.h"
// #include "driver/gpio.h"
// #include "esp_log.h"

// #include "portmacro.h"
// #include "rgb_led.h"

/**
 * Application entry point
 */

#include "nvs_flash.h"

#include "wifi_app.h"

void app_main(void)
{
    // while (true) {
    //     rgb_led_wifi_app_started();
    //     //sleep(1);
    //     ESP_LOGI("my_tag", "wifi_app_started");
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    //     rgb_led_http_server_started();
    //     ESP_LOGI("my_tag", "http_server_started");
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    //     rgb_led_wifi_connected();	 
    //     ESP_LOGI("my_tag", "wifi connected");
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Start WiFi
    // wifi_app_start();

    //Example 2
    wifi_init();
    
}
