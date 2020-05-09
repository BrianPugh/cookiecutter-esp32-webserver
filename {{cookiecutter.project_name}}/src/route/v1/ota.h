#ifndef PROJECT_OTA_H__
#define PROJECT_OTA_H__

#include "server.h"

/**
 * @brief Route for handling push OTA
 */
esp_err_t ota_post_handler(httpd_req_t *req);

#endif
