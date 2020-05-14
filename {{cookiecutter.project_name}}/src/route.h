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


/**
 * Send the contents of the file that was stored as binary/text data
 */
#define HTTP_SEND_BINARY(req, name) do {                                        \
    extern const unsigned char script_start[] asm("_binary_" name "_start");       \
    extern const unsigned char script_end[]   asm("_binary_" name "_end");         \
    const size_t script_size = (script_end - script_start);                     \
    httpd_resp_send_chunk(req, (const char *)script_start, script_size);        \
}while(0)


/**
 * @brief Send some binary data wrapped in script brackets.
 */
#define HTTP_SEND_SCRIPT(req, name) do{                                         \
    httpd_resp_sendstr_chunk(req, "<script>");                                  \
    HTTP_SEND_BINARY(req, name);                                                \
    httpd_resp_sendstr_chunk(req, "</script>");                                 \
}while(0)


#define HTTP_SEND_JS(req, name) do{                                             \
    HTTP_SEND_SCRIPT(req, name "_js" );                                         \
}while(0)

#endif
