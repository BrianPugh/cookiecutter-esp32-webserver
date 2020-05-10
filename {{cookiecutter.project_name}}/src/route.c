#include "route.h"

/* Include route handlers */
#include "route/v1/example.h"
#include "route/v1/filesystem.h"
#include "route/v1/nvs.h"
#include "route/v1/ota.h"
#include "route/v1/system.h"

__unused static const char TAG[] = "route";


#define ERR_CHECK(x)                   \
    do {                               \
        err = x;                       \
        if( ESP_OK != err ) goto exit; \
    } while(0)


esp_err_t register_routes() {
	/* Add all routes HERE */
    esp_err_t err = ESP_OK;

    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM "/*", HTTP_DELETE, filesystem_file_delete_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM "/*", HTTP_GET, filesystem_file_get_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM, HTTP_GET, filesystem_file_get_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM "/*", HTTP_POST, filesystem_file_post_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_NVS "/*", HTTP_POST, nvs_post_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_NVS "/*", HTTP_GET, nvs_get_handler));
    ERR_CHECK(server_register("/api/v1/led/timer", HTTP_POST, led_timer_post_handler));
    ERR_CHECK(server_register("/api/v1/ota", HTTP_POST, ota_post_handler));
    ERR_CHECK(server_register("/api/v1/system/info", HTTP_GET, system_info_get_handler));

exit:
    return err;
}


esp_err_t parse_post_request(cJSON **json, httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((server_ctx_t *)(req->user_ctx))->scratch;
    int received = 0;
    ESP_LOGI(TAG, "Parsing POST request of length %d", total_len);

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
    *json = cJSON_Parse(buf);
    if( NULL == *json) {
        ESP_LOGE(TAG, "Invalid json data: %s", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse JSON data");
        return ESP_FAIL;
    }

    return ESP_OK;
}

