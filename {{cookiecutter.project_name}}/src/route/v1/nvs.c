#include "route/v1/nvs.h"

esp_err_t nvs_post_handler(httpd_req_t *req)
{
    
    esp_err_t err = ESP_FAIL;
    cJSON *root;
    if(ESP_OK != parse_post_request(&root, req)) {
        goto exit;
    }

    //TODO
    
exit:
    return err;
}

esp_err_t nvs_key_get_handler(httpd_req_t *req)
{
    // TODO
    esp_err_t err = ESP_FAIL;
    return err;
}

esp_err_t nvs_root_get_handler(httpd_req_t *req)
{
    // TODO
    esp_err_t err = ESP_FAIL;
    return err;
}

