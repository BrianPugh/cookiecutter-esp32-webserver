#include "server.h"
#include "route.h"

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

    ERR_CHECK(register_routes() == ESP_OK, "Failed to register routes");

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


esp_err_t parse_post_request(cJSON *json, httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((server_ctx_t *)(req->user_ctx))->scratch;
    int received = 0;

    if (total_len >= CONFIG_SERVER_SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    json = cJSON_Parse(buf);
    if( NULL == json) {
        ESP_LOGE(TAG, "Invalid json data: %s", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse JSON data");
        return ESP_FAIL;
    }

    return ESP_OK;
}

