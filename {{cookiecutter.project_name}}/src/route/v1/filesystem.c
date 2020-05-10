#include "errno.h"
#include "route/v1/filesystem.h"
#include "../../filesystem.h"
#include <sys/param.h>

/**
 * largely based off of:
 *     https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/file_serving/main/file_server.c
 */


__unused static const char TAG[] = "route/v1/filesystem";


#define MAX_FILE_SIZE   (500*1024) // 500 KB
#define MAX_FILE_SIZE_STR "500KB"
#define MAX_FILE_PATH 256

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)


static int mkdir_if_not_exist(const char *path) {
    struct stat sb;
    if (stat(path, &sb) == 0) {
        //Directory or file exists
        
        if(S_ISDIR(sb.st_mode)) {
            // Is a directory
            // Do Nothing
        }
        else {
            // Was a file
            return -1;
        }
    }
    else {
        // Directory doesn't exist, make it
        if ( 0 != mkdir(path, S_IRWXU) ) {
            return -1;
        }
    }
    return 0;
}


/**
 * @brief modifies path in place to remove repeated '/'
 */
static char *trim_separators(char *path) {
    uint32_t count = 0;
    char *w, *r;
    for(w = path, r = path; *r != '\0'; r++) {
        if( *r == '/' ) count++;
        else count = 0;

        if(count > 1) {
            continue;
        }

        *w++ = *r;
    }
    *w = '\0';
    return path;
}

/**
 * @brief Recursively create parent directories for a given path
 * @param[in] path Path to a file/directory to create paths up to
 * @param[in] isfile If `true`, do not create the last token.
 * @returns 0 on success. On failure, a partial directory structure may have
 * been created.
 */
static int mkdir_p(const char *path, bool isfile)
{
    int err = -1;
    char *_path = NULL;

    _path = strdup(path);
    if(NULL == _path) goto exit;

    errno = 0;
    for (char *p = _path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if(0 != mkdir_if_not_exist(_path)){
                goto exit;
            }
            *p = '/';
        }
    }   

    if( !isfile && 0 != mkdir_if_not_exist(_path)) {
        goto exit;
    }

    err = 0;

exit:
    if(_path) free(_path);
    return err;
}


/**
 * @brief Delete a file/folder and all its contents
 *
 * Based on https://stackoverflow.com/a/42596507
 */
int rm_rf(const char path[]){
    size_t path_len;
    char *full_path = NULL;
    DIR *dir = NULL;
    struct stat stat_path, stat_entry;
    struct dirent *entry;
    int err = -1;

    // stat for the path
    err = stat(path, &stat_path);
    if( err ) goto exit;

    // If its a file, just delete it
    if (S_ISDIR(stat_path.st_mode) == 0) {
        err = unlink(path);
        goto exit;
    }

    // if not possible to read the directory for this user
    if ((dir = opendir(path)) == NULL) {
        ESP_LOGE(TAG, "Can`t open directory %s", path);
        err = 1;
        goto exit;
    }

    // the length of the path
    path_len = strlen(path);

    // iteration through entries in the directory
    while ((entry = readdir(dir)) != NULL) {

        // skip entries "." and ".."
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        // determinate a full path of an entry
        full_path = calloc(path_len + strlen(entry->d_name) + 2, sizeof(char));
        strcpy(full_path, path);
        strcat(full_path, "/");
        strcat(full_path, entry->d_name);

        // stat for the entry
        stat(full_path, &stat_entry);

        err = rm_rf(full_path);
        if(err) goto exit;

        free(full_path);
    }

    closedir(dir);
    dir = NULL;

    // remove the devastated directory and close the object of it
    err = rmdir(path);
    if(err) {
        ESP_LOGE(TAG, "Can`t remove directory: %s\n", path);
        goto exit;
    }

    err = 0;

exit:
    if(dir) closedir(dir);
    return err;
}

/**
 * @brief get the local system pathname from the URI.
 *
 * Caller must free returned string
 */
static char *get_path_from_uri(const httpd_req_t *req) {
    // Where the filesystem is mounted; e.g. "/fs"
	const char *base_path = ((server_ctx_t *)req->user_ctx)->base_path;
    const char *uri = trim_separators(req->uri);
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

    //TODO pickup here
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

    /* Send HTML file header */
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

    /* Get handle to embedded file upload script */
    /* See EMBED_FILES in CMakeLists.txt */
    extern const unsigned char upload_script_start[] asm("_binary_api_v1_filesystem_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_api_v1_filesystem_html_end");

    const size_t upload_script_size = (upload_script_end - upload_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
        "<tbody>");

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

    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

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

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

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

