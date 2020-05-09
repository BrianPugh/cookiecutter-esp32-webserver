#include "route/v1/example.h"
#include "led.h"

static const char TAG[] = "route/v1/example";


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

