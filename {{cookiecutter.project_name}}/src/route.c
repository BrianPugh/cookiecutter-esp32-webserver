#include "route.h"

/**************************
 * Private Route Handlers *
 **************************/

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/********************
 * Public Functions *
 ********************/

#define ERR_CHECK(x)                   \
    do {                               \
        err = x;                       \
        if( ESP_OK != err ) goto exit; \
    } while(0)

esp_err_t register_routes() {
	/* Add all routes HERE */
    esp_err_t err = ESP_OK;

    ERR_CHECK(server_register("/api/v1/system/info", HTTP_GET, system_info_get_handler));

exit:
    return err;
}


