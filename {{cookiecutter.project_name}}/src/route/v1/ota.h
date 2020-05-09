#ifndef PROJECT_OTA_H__
#define PROJECT_OTA_H__

#include "route.h"


/**
 * @brief Route for handling push OTA
 *
 * Will restart the device upon successful flashing
 *
 * To update via curl:
 *      curl -X POST ${ESP32_IP}/api/v1/ota --data-binary @- < build/${PROJECT_NAME}.bin
 */
esp_err_t ota_post_handler(httpd_req_t *req);


#endif
