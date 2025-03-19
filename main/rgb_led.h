/*
 * rgb_led.h
 *
 *  Created on: 03-Dec-2024
 *      Author: SHASHANK R
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

// RGB LED GPIOs
#define RGB_LED_RED_GPIO	21
#define RGB_LED_GREEN_GPIO	21
#define RGB_LED_BLUE_GPIO	21

//RGB LED color mix channels
#define RGB_LED_CHANNEL_NUM	3

// RGB LED configuration
typedef struct
{
	int channel;
	int gpio;
	int mode;
	int timer_index;	
} ledc_info_t;

//ledc_info_t ledc_ch[RGB_LED_CHANNEL_NUM]; 

/**
Colors to indicate WiFi application has started.
*/
void rgb_led_wifi_app_started(void);

/**
Colors to indicate HTTP server has started.
*/
void rgb_led_http_server_started(void);

/**
Colors to indicate that the ESP32 is connected to the access point.
*/
void rgb_led_wifi_connected(void);

#endif /* MAIN_RGB_LED_H_ */
