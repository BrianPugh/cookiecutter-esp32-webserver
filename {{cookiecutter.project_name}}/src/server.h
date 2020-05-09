#ifndef PROJECT_SERVER_H__
#define PROJECT_SERVER_H__

#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "fcntl.h"
#include "string.h"

#define CONFIG_SERVER_SCRATCH_BUFSIZE (10240)

typedef struct server_ctx {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[CONFIG_SERVER_SCRATCH_BUFSIZE];
} server_ctx_t;

extern server_ctx_t *server_ctx;


/***
 * @brief Initialize and start the server
 *
 * Requires a wifi connection to be already established.
 *
 * @param[in] base_path Base path to a mounted filesystem to be available
 *            in the server context.
 */
esp_err_t server_init(const char *base_path);


/****
 * @brief Register a handler for a route
 * @param[in] route the route e.g. "/api/v1/test"
 * @param[in] method like HTTP_GET, HTTP_POST, HTTP_DELETE, HTTP_PUT
 * @param[in] handler Callback to handle requests
 */
esp_err_t server_register(const char *route, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r));


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
esp_err_t parse_post_request(cJSON *json, httpd_req_t *req);

#endif
