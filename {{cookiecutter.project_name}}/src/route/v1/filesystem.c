#include "route/v1/filesystem.h"
#include "../../filesystem.h"
#include <sys/param.h>

/**
 * largely based off of:
 *     https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/file_serving/main/file_server.c
 */


__unused static const char TAG[] = "route/v1/filesystem";


/**
 * @brief get the local system pathname from the URI.
 *
 * Caller must free returned string
 */
static char *get_path_from_uri(const httpd_req_t *req) {
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

    if (pathlen <= strlen(PROJECT_ROUTE_V1_FILESYSTEM)) {
		// Just return the root of the mounted filesystem
        // For whenever the endpt "/api/v1/filesystem" is queried without
        // a trailing "/"
        pathlen = 0;
	}
    else {
        // Move URI pointer to start at the filepath portion
        // The plus one is for the trailing '/'
        uri += (strlen(PROJECT_ROUTE_V1_FILESYSTEM) + 1);
        pathlen -= (strlen(PROJECT_ROUTE_V1_FILESYSTEM) + 1);
    }

    // +1 for null-terminator
    // +1 for initial slash separator
    size_t parsed_len = strlen(base_path) + pathlen + 2;
    ESP_LOGI(TAG, "Allocating %d bytes for path", parsed_len);
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

    ESP_LOGI(TAG, "Parsed Path: %s", parsed);

exit:
    return parsed;
}


/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    esp_err_t err = ESP_FAIL;
    char entrypath[MAX_FILE_PATH];
    char entrysize[16];
    const char *entrytype;
    bool serve_html;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = NULL;

    dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        goto exit;
    }

    serve_html = detect_if_browser(req);

    if(serve_html) {
        /* Send HTML file header */
        httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

        /* Add file upload form and script which on execution sends a POST request to /upload */
        HTTP_SEND_BINARY(req, api_v1_filesystem_html);

        /* Send file-list table definition and column labels */
        httpd_resp_sendstr_chunk(req,
            "<table class=\"fixed\" border=\"1\">"
            "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
            "<thead><tr>"
            "<th>Name</th>"
            "<th>Type</th>"
            "<th>Size (Bytes)</th>"
            "<th>Delete</th>"
            "</tr></thead><tbody>");

        /* Add ".." option at top if we are not at the root of the mount point*/
        if(strcmp(dirpath, CONFIG_PROJECT_FS_MOUNT_POINT "/")){
            char *p;
            char *parent = NULL;
            parent = strdup(req->uri);
            if(NULL == parent) goto exit;
            parent = trim_separators(parent);
            for(p = parent + strlen(parent) - 2; *p != '/'; p--) ;
            p[1] = '\0';
            httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
            httpd_resp_sendstr_chunk(req, parent);  // link
            httpd_resp_sendstr_chunk(req, "\">..</a></td><td>");
            free(parent);
        }
    }
    else {
        /* Send JSON Meta */
        httpd_resp_sendstr_chunk(req, "{\"contents\":[");
    }

    /* Iterate over all files / folders and fetch their names and sizes */
    bool first_iter = true;
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        if(serve_html) {
            /* Send chunk of HTML file containing table entries with file name and size */
            httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
            httpd_resp_sendstr_chunk(req, req->uri);
            httpd_resp_sendstr_chunk(req, "/");
            httpd_resp_sendstr_chunk(req, entry->d_name);
            if (entry->d_type == DT_DIR) {
                httpd_resp_sendstr_chunk(req, "/");
            }
            httpd_resp_sendstr_chunk(req, "\">");
            httpd_resp_sendstr_chunk(req, entry->d_name);
            httpd_resp_sendstr_chunk(req, "</a></td><td>");
            httpd_resp_sendstr_chunk(req, entrytype);
            httpd_resp_sendstr_chunk(req, "</td><td>");
            httpd_resp_sendstr_chunk(req, entrysize);
            httpd_resp_sendstr_chunk(req, "</td><td>");
            httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"");
            httpd_resp_sendstr_chunk(req, req->uri);
            httpd_resp_sendstr_chunk(req, "/");
            httpd_resp_sendstr_chunk(req, entry->d_name);
            httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
            httpd_resp_sendstr_chunk(req, "</td></tr>\n");
        }
        else {
            if(!first_iter) {
                httpd_resp_sendstr_chunk(req, ",");
            }
            httpd_resp_sendstr_chunk(req, "{\"name\":\"");
            httpd_resp_sendstr_chunk(req, entry->d_name);
            httpd_resp_sendstr_chunk(req, "\",\"type\":\"");
            if (entry->d_type == DT_DIR) {
                httpd_resp_sendstr_chunk(req, "dir");
            }
            else{
                httpd_resp_sendstr_chunk(req, "file");
            }
            httpd_resp_sendstr_chunk(req, "\",\"size\":");
            httpd_resp_sendstr_chunk(req, entrysize);
            httpd_resp_sendstr_chunk(req, "}");
        }
        first_iter = false;
    }

    if(serve_html){
        /* Finish the file list table */
        httpd_resp_sendstr_chunk(req, "</tbody></table>");

        /* Send remaining chunk of HTML file to complete it */
        httpd_resp_sendstr_chunk(req, "</body></html>");
    }
    else{
        httpd_resp_sendstr_chunk(req, "]}");
    }

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    err = ESP_OK;

exit:
    if(dir) closedir(dir);
    return err;
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

esp_err_t filesystem_file_post_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    FILE *fd = NULL;
    char *buf = ((server_ctx_t *)req->user_ctx)->scratch;

    char *filepath = get_path_from_uri(req);

    if (!filepath || filepath[strlen(filepath) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filepath : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filepath");
        goto exit;
    }

    // Enable the following if you don't want POST requests to be able to
    // overwrite existing files.
#if 0
    struct stat file_stat;
    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        goto exit;
    }
#endif
    ESP_LOGI(TAG, "Content_len: %d", req->content_len);

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        goto exit;
    }

    if(req->content_len == 0) {
        /* Delete the file. NOTE: this means that we don't allow
         * the upload of 0-byte files. */
        err = filesystem_file_delete_handler(req);
        goto exit;
    }

    /* Create folders to path if necessary. */
    if( 0 != mkdir_p(filepath, true) ) {
        ESP_LOGE(TAG, "Failed to create directories for: %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create directories");
        goto exit;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        goto exit;
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
            goto exit;
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
            goto exit;
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
    httpd_resp_set_hdr(req, "Location", "/api/v1/filesystem/");
    httpd_resp_sendstr(req, "File uploaded successfully");

    err = ESP_OK;

exit:
	if(filepath) {
        free(filepath);
	}
	return err;
}


esp_err_t filesystem_file_get_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    FILE *fd = NULL;
    struct stat file_stat;
    char *chunk = ((server_ctx_t *)req->user_ctx)->scratch;

    char *filepath = get_path_from_uri(req);

    if (!filepath) {
        ESP_LOGE(TAG, "Invalid filepath : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filepath");
        goto exit;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filepath[strlen(filepath) - 1] == '/') {
        err = http_resp_dir_html(req, filepath);
        goto exit;
    }

    if (stat(filepath, &file_stat) == -1) {
		// TODO: re-enable if useful
#if 0
        /* If file not present on filesystem, check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filepath, "/index.html") == 0) {
            err = index_html_get_handler(req);
            goto exit;
        } else if (strcmp(filepath, "/favicon.ico") == 0) {
            err = favicon_get_handler(req);
            goto exit;
        }
#endif
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        goto exit;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        goto exit;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filepath, file_stat.st_size);
    set_content_type_from_file(req, filepath);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, CONFIG_SERVER_SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                goto exit;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);

    err = ESP_OK;

exit:
    if(filepath) {
        free(filepath);
    }
    if(fd) {
        fclose(fd);
    }
    return err;
}


esp_err_t filesystem_file_delete_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    struct stat file_stat;

    char *filepath = get_path_from_uri(req);

    if (!filepath) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Does not exist: %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File/Directory does not exist");
        goto exit;
    }

    ESP_LOGI(TAG, "Deleting: %s", filepath);

    /* Delete file */
    rm_rf(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", PROJECT_ROUTE_V1_FILESYSTEM);
    httpd_resp_sendstr(req, "File deleted successfully");
	err = ESP_OK;

exit:
    if(filepath) {
        free(filepath);
    }
    return err;
}

