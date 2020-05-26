#include "route/v1/system.h"
#include "esp_ota_ops.h"
#include "sodium.h"

__unused static const char TAG[] = "route/v1/system";


/* Simple handler for getting system handler */
esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    {
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        cJSON_AddStringToObject(root, "idf-version", IDF_VER);
        const char *model_str;
        #define CHIP_CASE(x) case CHIP_ ## x:model_str = #x; break;
        switch(chip_info.model) {
            CHIP_CASE(ESP32)
            CHIP_CASE(ESP32S2)
            default:
                model_str = "UNKNOWN";
        }
        #undef CHIP_CASE
        cJSON_AddStringToObject(root, "model", model_str);
        cJSON_AddNumberToObject(root, "cores", chip_info.cores);
        cJSON_AddNumberToObject(root, "silicon-revision", chip_info.revision);
    }
    {
        const esp_app_desc_t *desc = esp_ota_get_app_description();
        cJSON_AddStringToObject(root, "project-name", desc->project_name);
        cJSON_AddStringToObject(root, "project-version", desc->version);
        cJSON_AddStringToObject(root, "compile-date", desc->date);
        cJSON_AddStringToObject(root, "compile-time", desc->time);
        cJSON_AddNumberToObject(root, "secure-version", desc->secure_version);

        {
            char hex[65] = { 0 };
            sodium_bin2hex(hex, sizeof(hex), desc->app_elf_sha256, 32);
            cJSON_AddStringToObject(root, "app-elf-sha256", hex);
        }
    }
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}


esp_err_t system_reboot_post_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "Rebooting system...");
    vTaskDelay(100 / portTICK_PERIOD_MS);  // give it enough time to send the http message
    esp_restart();
    return ESP_OK;
}


esp_err_t system_time_get_handler(httpd_req_t *req)
{
    char buf[128] = { 0 };
    time_t now;
    time(&now);
    snprintf(buf, sizeof(buf), "{\"time\":%ld}", now);
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}
