#include "ota.h"
#include "esp_ota_ops.h"
#include <sys/param.h>


static const char TAG[] = "route/v1/ota";

esp_err_t ota_post_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    esp_ota_handle_t ota_handle = 0; 

    int cur_len = 0;  // Current length of all data received so far
    int recv_len = 0;  // Length of data received in this chunk
    int total_len = req->content_len;  // Total data size

    char *buf = ((server_ctx_t *)(req->user_ctx))->scratch;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    ESP_ERROR_CHECK( esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) );

    ESP_LOGI(TAG, "Firwmare Upload Begin: Going to transfer %d bytes.", total_len);

    do {
        if ((recv_len = httpd_req_recv(req, buf, MIN(total_len - cur_len, CONFIG_SERVER_SCRATCH_BUFSIZE))) <= 0) {
            /* Error Handling */
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* Some irrecoverable error: cancel OTA */
            ESP_LOGE(TAG, "Irrecoverable error receiving data. Aborting...");
            goto exit;
        }
        err = esp_ota_write(ota_handle, buf, recv_len);
        if( ESP_OK != err ) {
            // TODO
            ESP_LOGE(TAG, "Failed to write OTA chunk to partition. Aborting...");
            goto exit;
        }
        cur_len += recv_len;
        ESP_LOGI(TAG, "upload progress: %.1f%%", (100.0 * cur_len / total_len));
    } while (recv_len > 0 && cur_len < total_len);

    ESP_LOGI(TAG, "Firmware Upload Complete. Tranferred %d bytes", cur_len);

    ESP_ERROR_CHECK( esp_ota_end(ota_handle) );
    ESP_ERROR_CHECK( esp_ota_set_boot_partition(update_partition) );
    err = ESP_OK;

    const char msg[] = "OTA Successful; rebooting...";
    httpd_resp_send(req, msg, strlen(msg));
    vTaskDelay(100 / portTICK_PERIOD_MS);  // give it enough time to send the http message
    esp_restart();

exit:
    /* Abort */
    if ( ota_handle > 0) {
        esp_ota_end(ota_handle);  // Free up resources
    }
    return err;
}
