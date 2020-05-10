/***
 * CRUD interface for interacting with the filesystem.
 */

#ifndef PROJECT_ROUTE_V1_FILESYSTEM_H__
#define PROJECT_ROUTE_V1_FILESYSTEM_H__

#include "route.h"


#define PROJECT_FILESYSTEM_ROUTE_ROOT "/api/v1/filesystem"

/**
 * @brief Upload/overwrite file
 *
 *      curl -X POST ${ESP32_IP}/api/v1/filesystem/${PATH} --data-binary @- < ${LOCAL_FILE}
 *
 * where:
 *     ESP32_IP - IP address of the device
 *     PATH - Path on device
 *     LOCAL_FILE - File to upload
 */
esp_err_t filesystem_file_post_handler(httpd_req_t *req);


/**
 * @brief Get a file
 *
 *     curl ${ESP32_IP}/api/v1/filesystem/${PATH}
 * where:
 *     PATH - Path on device
 */
esp_err_t filesystem_file_get_handler(httpd_req_t *req);


/**
 * @brief Delete a file
 *
 *     curl ${ESP32_IP}/api/v1/filesystem/${PATH}
 * where:
 *     PATH - Path on device
 */
esp_err_t filesystem_file_delete_handler(httpd_req_t *req);

#endif
