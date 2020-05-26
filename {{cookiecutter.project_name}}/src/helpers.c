#include "nvs.h"
#include "nvs_flash.h"
#include "helpers.h"


char *nvs_get_str_default(const char *namespace, const char *key, const char *def)
{
    char *value = NULL;
    esp_err_t err;
    nvs_handle_t h = 0;
    size_t len;

    err = nvs_open(namespace, NVS_READWRITE, &h);
    if(ESP_OK != err) goto exit;

    err = nvs_get_str(h, key, NULL, &len);
    if(ESP_ERR_NVS_NOT_FOUND == err) {
        /* Store the default value to nvs */
        ESP_ERROR_CHECK(nvs_set_str(h, key, def));
        err = nvs_get_str(h, key, NULL, &len);
    }
    if(ESP_OK != err) goto exit;

    if(NULL == (value = malloc(len))) goto exit;

    err = nvs_get_str(h, key, value, &len);
    if(ESP_OK != err) goto exit;

    err = ESP_OK;

exit:
    if(h) nvs_close(h);
    return value;
}
