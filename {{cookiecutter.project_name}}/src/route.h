#ifndef PROJECT_ROUTE_H__
#define PROJECT_ROUTE_H__

#include "server.h"

/**
 * @brief Register all routes to the httpd_server
 */
esp_err_t register_routes();


/**
 * @brief Parse a POST request into a cJSON object.
 *
 * Uses context scratch pad. The scratch pad must not be cleared/re-used
 * during the lifetime of the json object.
 *
 * @param[out] json Parsed json object. Must be eventually be deleted via `cJSON_Delete`
 *          by the caller.
 * @param[in] req Some POST request
 * @returns cJSON object. Must be eventually be deleted via `cJSON_Delete`
 *          by the caller. Returns NULL on error.
 */
esp_err_t parse_post_request(cJSON **json, httpd_req_t *req);


/**
 * @brief Detects if requester was a browser or not.
 *
 * Currently does this by checking if the first entry in the ACCEPT field is
 * `text/html`
 *
 * @returns true if browser, false otherwise.
 */
bool detect_if_browser(httpd_req_t *req);

#endif
