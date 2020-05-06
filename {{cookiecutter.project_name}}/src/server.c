#include "server.h"

static const char *TAG = "server";

/* Checks if first arg is `false`; if so, log and go to `exit` */
#define ERR_CHECK(a, str, ...)                                                   \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto exit;                                                            \
        }                                                                         \
    } while (0)



server_ctx_t *server_ctx = NULL;
static httpd_handle_t server = NULL;

/**
 * @brief Start Web server
 *
 * All Routes must be registered first
 *
 * @param[in] base_path Path to the root of the filesystem
 */
esp_err_t server_init(const char *base_path) {

    /* Environment Validation */
    if( server_ctx || server ) {
        ESP_LOGE(TAG, "server_init has already been called");
        return ESP_FAIL;
    }

    /* Input Validation */
    ERR_CHECK(base_path, "wrong base path");

    /* Allocate and populate server context */
    server_ctx = calloc(1, sizeof(server_ctx_t));
    ERR_CHECK(server_ctx, "OOM while allocating server context");
    strlcpy(server_ctx->base_path, base_path, sizeof(server_ctx->base_path));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server");
    ERR_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed");

    return ESP_OK;

exit:
    if( NULL!= server_ctx ) {
        free(server_ctx);
    }
    return ESP_FAIL;
}

esp_err_t server_register(const char *route, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r))
{
    httpd_uri_t desc;
    desc.uri = route;
    desc.method = method;
    desc.handler = handler;
    desc.user_ctx = server_ctx;

    return httpd_register_uri_handler(server, &desc);
}
