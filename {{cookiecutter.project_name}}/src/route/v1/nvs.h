/***
 * RU interface for interacting with non-volatile storage key/value pairs.
 */


#ifndef PROJECT_ROUTE_V1_NVS_H__
#define PROJECT_ROUTE_V1_NVS_H__

#include "route.h"

#define PROJECT_ROUTE_V1_NVS "/api/v1/nvs"


/**
 * @brief Update NVS key/value pairs. 
 *
 *      curl -X POST ${ESP32_IP}/api/v1/nvs --data '{"key": value}'
 *
 * where:
 *     ESP32_IP - IP address of the device
 *     "key" - key to store under
 *     "value" - value to store at key
 *
 * Datatype is interpretted from the json value datatype. Binary data
 * is currently not supported.
 */
esp_err_t nvs_post_handler(httpd_req_t *req);

/**
 * @brief Get a NVS value
 *
 *     curl ${ESP32_IP}/api/v1/nvs
 *     curl ${ESP32_IP}/api/v1/nvs/${NAMESPACE}
 *     curl ${ESP32_IP}/api/v1/nvs/${NAMESPACE}/${KEY}
 *
 * where:
 *     KEY - NVS key to look up
 */
esp_err_t nvs_get_handler(httpd_req_t *req);


#endif
