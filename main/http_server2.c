/*
 * http_server.c
 *
 *  Created on: 17-Dec-2024
 *      Author: SHASHANK R
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "sys/param.h"
#include "esp_timer.h"

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include <cJSON.h>

// Tag used for ESP serial console message
static const char TAG[] = "http_server2";

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
		.callback = &http_server_fw_update_reset_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;


/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;

	for (;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");

					break;

				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");

					break;

				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");

					break;

				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
                    g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
                    http_server_fw_update_reset_timer();
					break;

				case HTTP_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
                    g_fw_update_status = OTA_UPDATE_FAILED;
					break;

				default:
					break;
			}
		}
	}
}


/**
 * Receives the .bin file fia the web page and handles the firmware update
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started.
 */
esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Received HTTP Method: %d", req->method);
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	bool flash_successful = false;

	// Enable CORS by adding headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    // Handle CORS preflight request
    if (req->method == HTTP_OPTIONS)
    {
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	if (!update_partition)
    {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Partition Not Found");
        return ESP_FAIL;
    }

	ESP_LOGI(TAG, "Starting OTA update...\nOTA Update Start: Total Size = %d bytes", content_length);


	// do
	// {
	// 	// Read the data for the request
	// 	if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
	// 	{
	// 		// Check if timeout occurred
	// 		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
	// 		{
	// 			ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
	// 			continue; ///> Retry receiving if timeout occurred
	// 		}
	// 		ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
	// 		return ESP_FAIL;
	// 	}
	// 	printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

	// 	// Is this the first data we are receiving
	// 	// If so, it will have the information in the header that we need.
	// 	if (!is_req_body_started)
	// 	{
	// 		is_req_body_started = true;

	// 		// Get the location of the .bin file content (remove the web form data)
	// 		char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
	// 		int body_part_len = recv_len - (body_start_p - ota_buff);

	// 		printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

	// 		esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
	// 		if (err != ESP_OK)
	// 		{
	// 			printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
	// 			return ESP_FAIL;
	// 		}
	// 		else
	// 		{
	// 			printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, update_partition->address);
	// 		}

	// 		// Write this first part of the data
	// 		esp_ota_write(ota_handle, body_start_p, body_part_len);
	// 		content_received += body_part_len;
	// 	}
	// 	else
	// 	{
	// 		// Write OTA data
	// 		esp_ota_write(ota_handle, ota_buff, recv_len);
	// 		content_received += recv_len;
	// 	}

	// } while (recv_len > 0 && content_received < content_length);

	// if (esp_ota_end(ota_handle) == ESP_OK)
	// {
	// 	// Lets update the partition
	// 	if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
	// 	{
	// 		const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
	// 		ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
	// 		flash_successful = true;
	// 	}
	// 	else
	// 	{
	// 		ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
	// 	}
	// }
	// else
	// {
	// 	ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
	// }

	// // We won't update the global variables throughout the file, so send the message about the status
	// if (flash_successful) { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL); } else { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); }

	// return ESP_OK;

	esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error starting OTA update: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    do {
        // Read the data for the request
        recv_len = httpd_req_recv(req, ota_buff, MIN(content_length - content_received, sizeof(ota_buff)));

        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Socket Timeout, retrying...");
                continue; // Retry receiving if timeout occurred
            }
            ESP_LOGE(TAG, "OTA Receive Error: %d", recv_len);
            return ESP_FAIL;
        }

        printf("Received: %d/%d bytes\r", content_received + recv_len, content_length);

        // Process header only for first received chunk
        if (!is_req_body_started) {
            is_req_body_started = true;

            // Find start of binary data in the request
            char *body_start_p = strstr(ota_buff, "\r\n\r\n");
            if (!body_start_p) {
                ESP_LOGE(TAG, "Invalid request format: No header-body separator found!");
                return ESP_FAIL;
            }
            body_start_p += 4; // Move past the separator

            int body_part_len = recv_len - (body_start_p - ota_buff);

            ESP_LOGI(TAG, "Writing to OTA partition at offset 0x%lx", update_partition->address);

            if (esp_ota_write(ota_handle, body_start_p, body_part_len) != ESP_OK) {
                ESP_LOGE(TAG, "Error writing OTA data!");
                return ESP_FAIL;
            }
            content_received += body_part_len;
        } else {
            // Write subsequent OTA data
            if (esp_ota_write(ota_handle, ota_buff, recv_len) != ESP_OK) {
                ESP_LOGE(TAG, "Error writing OTA data!");
                return ESP_FAIL;
            }
            content_received += recv_len;
        }

    } while (recv_len > 0 && content_received < content_length);

    if (esp_ota_end(ota_handle) == ESP_OK) {
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(TAG, "OTA Update Successful! Next boot partition at offset 0x%lx", boot_partition->address);
            flash_successful = true;
        } else {
            ESP_LOGE(TAG, "Failed to set boot partition!");
        }
    } else {
        ESP_LOGE(TAG, "OTA update failed!");
    }

    // Send OTA update status
    http_server_monitor_send_message(flash_successful ? HTTP_MSG_OTA_UPDATE_SUCCESSFUL : HTTP_MSG_OTA_UPDATE_FAILED);

    return flash_successful ? ESP_OK : ESP_FAIL;

}

/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
	// char otaJSON[100];

	// ESP_LOGI(TAG, "OTAstatus requested");

	// sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	// httpd_resp_set_type(req, "application/json");
	// httpd_resp_send(req, otaJSON, strlen(otaJSON));

	// return ESP_OK;

	cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "compile_date", __DATE__);
    cJSON_AddStringToObject(response, "compile_time", __TIME__);
    cJSON_AddNumberToObject(response, "ota_update_status", g_fw_update_status);

    char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));

    cJSON_Delete(response);
    free(response_str);
    
    return ESP_OK;
}

/**
 * Sets up the deafult http server configuration
 * @return http server instance handle if successful, NULL otherwise
 */
static httpd_handle_t http_server_configure(void)
{
	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// Create the message queue
	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));
	
	// Create HTTP server monitor task
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);


	// The core that the HTTP server will run on
	config.core_id = HTTP_SERVER_TASK_CORE_ID;

	// Adjust the default priority to 1 less than the wifi application task
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;

	// Bump up the stack size (default is 4096)
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;

	// Increase uri handlers
	config.max_uri_handlers = 20;

	// Increase the timeout limits
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;

	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			config.server_port,
			config.task_priority);

	// Start the httpd server
	if (httpd_start(&http_server_handle, &config) == ESP_OK)
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");


        // register OTAupdate handler
		httpd_uri_t OTA_update = {
				.uri = "/OTAupdate",
				.method = HTTP_POST,
				.handler = http_server_OTA_update_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);

		// ✅ Add handler for OPTIONS preflight requests
		httpd_uri_t ota_options_uri = {
			.uri = "/OTAupdate",
			.method = HTTP_OPTIONS,
			.handler = http_server_OTA_update_handler,
			.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &ota_options_uri);
		
		// register OTAstatus handler
		httpd_uri_t OTA_status = {
				.uri = "/OTAstatus",
				.method = HTTP_GET,
				.handler = http_server_OTA_status_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);

		return http_server_handle;
	}

	return NULL;
}

void http_server_start(void)
{
    if( http_server_handle == NULL)
    {
        http_server_handle = http_server_configure();
    }
}

void http_server_stop(void)
{
    if(http_server_handle)
    {
        httpd_stop(http_server_handle);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
        http_server_handle = NULL;
    }
    if(task_http_server_monitor)
    {
        vTaskDelete(task_http_server_monitor);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
        task_http_server_monitor = NULL;
    }
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}


