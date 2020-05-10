#include "route/v1/filesystem.h"
#include <sys/param.h>

/**
 * largely based off of:
 *     https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/file_serving/main/file_server.c
 */


__unused static const char TAG[] = "route/v1/filesystem";


#define MAX_FILE_SIZE   (500*1024) // 500 KB
#define MAX_FILE_SIZE_STR "500KB"

/**
 * @brief get the local system pathname from the URI.
 *
 * Caller must free returned string
 */
static const char *get_path_from_uri(const httpd_req_t *req) {
    // Where the filesystem is mounted; e.g. "/fs"
	const char *base_path = ((server_ctx_t *)req->user_ctx)->base_path;
    const char *uri = req->uri;
    char *parsed = NULL;

    size_t pathlen = strlen(uri);
    {
        const char *quest = strchr(uri, '?');
        if (quest) {
            pathlen = MIN(pathlen, quest - uri);
        }
    }
    {
        const char *hash = strchr(uri, '#');
        if (hash) {
            pathlen = MIN(pathlen, hash - uri);
        }
    }
    // Move URI pointer to start at the filepath portion
    uri += strlen(PROJECT_FILESYSTEM_ROUTE_ROOT);
    pathlen -= strlen(PROJECT_FILESYSTEM_ROUTE_ROOT);

    // +1 for null-terminator
    size_t parsed_len = strlen(base_path) + pathlen + 1;
    parsed = malloc(parsed_len);
    if( NULL == parsed ) goto exit;
    {
        char *p = parsed;
        strcpy(p, base_path);
        p += strlen(base_path);
        *p++ = '/';
        strncpy(p, uri, pathlen);
        p += pathlen;
        *p = '\0';
    }

    //TODO pickup here
    ESP_LOGI(TAG, "Parsed Path: %s", parsed);

exit:
    return parsed;
}


esp_err_t filesystem_file_post_handler(httpd_req_t *req)
{
    FILE *fd = NULL;
    struct stat file_stat;
    char *buf = ((server_ctx_t *)req->user_ctx)->scratch;

    const char *filepath = get_path_from_uri(req);
    if (!filepath) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "filepath too long");
        return ESP_FAIL;
    }

    /* filepath cannot have a trailing '/' */
    if (filepath[strlen(filepath) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filepath : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filepath");
        return ESP_FAIL;
    }

    // Enable the following if you don't want POST requests to be able to
    // overwrite existing files.
#if 0
    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }
#endif

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filepath);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, CONFIG_SERVER_SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File uploaded successfully");

    return ESP_OK;
}


esp_err_t filesystem_file_get_handler(httpd_req_t *req)
{
    //TODO
    return ESP_OK;
}


esp_err_t filesystem_file_delete_handler(httpd_req_t *req)
{
    //TODO
    return ESP_OK;
}

