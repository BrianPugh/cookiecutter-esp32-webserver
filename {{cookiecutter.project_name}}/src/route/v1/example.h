#ifndef PROJECT_ROUTE_V1_EXAMPLE_H__
#define PROJECT_ROUTE_V1_EXAMPLE_H__

#include "route.h"


/**
 * @brief Simple handler for getting system handler 
 */
esp_err_t system_info_get_handler(httpd_req_t *req);


/**
 * @brief Turns on an LED for "duration" milliseconds
 *
 * invoke via (replace ${ESP32_IP} with your device's IP address):
 *     curl -X POST ${ESP32_IP}/api/v1/led/timer --data '{"duration": 1000}'
 */
esp_err_t led_timer_post_handler(httpd_req_t *req);


#endif
