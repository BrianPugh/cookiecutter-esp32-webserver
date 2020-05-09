#include "route/v1/example.h"
#include "led.h"

static const char TAG[] = "route/v1/example";


/* Simple handler for getting system handler */
esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Turns on an LED for X milliseconds 
 *
 * invoke via (replace <ESP32_IP> with your device's IP address):
 *     curl -X POST <ESP32_IP>/api/v1/led/timer --data '{"duration": 1000}'
 *
 */
esp_err_t led_timer_post_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    cJSON *root = NULL;
    if(ESP_OK != parse_post_request(&root, req)) {
        goto exit;
    }

    cJSON *duration = NULL;
    duration = cJSON_GetObjectItem(root, "duration");
    if( NULL == duration ) {
        ESP_LOGE(TAG, "Failed to get field \"duration\"");
        goto exit;
    }

    int time_ms = duration->valueint;
    led_set(true);
    /* NOTE: Blocking in a handler prevents additional clients from being served.
             If this you only intend to serve 1 request at a time, this is
             fine. However, if you'd like to serve additional request while this
             action is being performed, it'd be better to either:
             1. Push an action on a queue to be performed by another task.
             2. Use a timer
     */
    vTaskDelay( time_ms / portTICK_PERIOD_MS);
    led_set(false);

    httpd_resp_sendstr(req, "Post control value successfully");

    err = ESP_OK;

exit:
    if( NULL != root ) {
        cJSON_Delete(root);
    }

    return err;
}

