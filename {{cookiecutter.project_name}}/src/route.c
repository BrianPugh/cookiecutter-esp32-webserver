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


/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char _route_favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char _route_favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (_route_favicon_ico_end - _route_favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)_route_favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}


/**
 * @Brief Manual sitemap
 */
static esp_err_t root_get_handler(httpd_req_t *req) {
    HTTP_SEND_DOCTYPE_HTML(req);
    HTTP_SEND_COMMON_HEAD(req, "{{cookiecutter.project_name}} NVS");
    httpd_resp_sendstr_chunk(req, "<body>");

    httpd_resp_sendstr_chunk(req, "<h1>Admin</h1>");
    httpd_resp_sendstr_chunk(req, "<p><a href=\"" PROJECT_ROUTE_V1_FILESYSTEM "\">Filesystem</a></p>");
    httpd_resp_sendstr_chunk(req, "<p><a href=\"" PROJECT_ROUTE_V1_NVS "\">Non-Volatile Storage</a></p>");

    httpd_resp_sendstr_chunk(req, "</body>");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}


esp_err_t register_routes() {
	/* Add all routes HERE */
    esp_err_t err = ESP_OK;

    ERR_CHECK(server_register("/", HTTP_GET, root_get_handler));
    ERR_CHECK(server_register("/favicon.ico", HTTP_GET, favicon_get_handler));

    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM "/*", HTTP_DELETE, filesystem_file_delete_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM "/*", HTTP_GET, filesystem_file_get_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM,      HTTP_GET, filesystem_file_get_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_FILESYSTEM "/*", HTTP_POST, filesystem_file_post_handler));

    ERR_CHECK(server_register(PROJECT_ROUTE_V1_NVS "/*", HTTP_POST, nvs_post_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_NVS "/*", HTTP_GET, nvs_get_handler));
    ERR_CHECK(server_register(PROJECT_ROUTE_V1_NVS,      HTTP_GET, nvs_get_handler));

    ERR_CHECK(server_register("/api/v1/led/timer",   HTTP_POST, led_timer_post_handler));
    ERR_CHECK(server_register("/api/v1/ota",         HTTP_POST, ota_post_handler));
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

bool detect_if_browser(httpd_req_t *req)
{
    char buf[10] = {0};
    httpd_req_get_hdr_value_str(req, "ACCEPT", buf, sizeof(buf));
    if(0 == strcmp(buf, "text/html")) {
        //hacky; generally this field starts with text/html, so this 
        //prevents us from having to allocate for the entire field.
        ESP_LOGI(TAG, "GET request came from a browser");
        return true;
    }
    return false;
}


