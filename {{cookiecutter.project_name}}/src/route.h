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
 * Send the contents of the file that was stored as binary/text data.
 * Don't put file in quotes.
 */
#define HTTP_SEND_BINARY(req, name) do {                                                         \
    extern const unsigned char _route_script_##name##_start[] asm("_binary_" #name "_start");    \
    extern const unsigned char _route_script_##name##_end[]   asm("_binary_" #name "_end");      \
    const size_t script_size = (_route_script_##name##_end - _route_script_##name##_start);      \
    httpd_resp_send_chunk(req, (const char *)_route_script_##name##_start, script_size);         \
}while(0)


/**
 * @brief Send some binary data wrapped in script brackets.
 * Don't put file in quotes.
 */
#define HTTP_SEND_SCRIPT(req, name) do{                                         \
    httpd_resp_sendstr_chunk(req, "<script>");                                  \
    HTTP_SEND_BINARY(req, name);                                                \
    httpd_resp_sendstr_chunk(req, "</script>");                                 \
}while(0)

/**
 * @brief Send some stored javacript.
 * Don't put file in quotes.
 */
#define HTTP_SEND_JS(req, name) do{                                             \
    HTTP_SEND_SCRIPT(req, name ## _js );                                        \
}while(0)

#define HTTP_SEND_CSS(req, name) do{                                            \
    httpd_resp_sendstr_chunk(req, "<style>");                                   \
    HTTP_SEND_SCRIPT(req, name ## _css );                                       \
    httpd_resp_sendstr_chunk(req, "</style>");                                  \
}while(0)


static inline void HTTP_SEND_FAVICON(httpd_req_t *req) {
    httpd_resp_sendstr_chunk(req, "<link rel=\"icon\" href=\"/favicon.ico\">"); \
}


/**
 * @brief Send a common header with a title.
 *
 * Sends:
 *     1. title
 *     2. common css
 *     3. favicon
 *
 * @param[in] req
 * @param[in] title May be NULL.
 */
static inline void HTTP_SEND_COMMON_HEAD(httpd_req_t *req, char *title) {
    httpd_resp_sendstr_chunk(req, "<head>");
    if(title) {
        httpd_resp_sendstr_chunk(req, "<title>");
        httpd_resp_sendstr_chunk(req, title);
        httpd_resp_sendstr_chunk(req, "</title>");
    }
    HTTP_SEND_CSS(req, common);
    HTTP_SEND_FAVICON(req);
    httpd_resp_sendstr_chunk(req, "</head>");
}


static inline void HTTP_SEND_DOCTYPE_HTML(httpd_req_t *req) {
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
}

#endif
