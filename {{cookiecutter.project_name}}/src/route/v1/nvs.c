#include "nvs.h"
#include "nvs_flash.h"
#include "route/v1/nvs.h"
#include "sodium.h"
#include "errno.h"
#include <sys/param.h>

// Including NULL-terminator
#define NAMESPACE_MAX 16
#define KEY_MAX 16

// Bitfield of possible outcomes while parsing URI
#define PARSE_NAMESPACE ( 1 << 0 )  // Namespace was parsed
#define PARSE_KEY       ( 1 << 1 )  // Key was parsed
#define PARSE_ERROR       ( 1 << 7 ) // Error during parsing

#define CJSON_CHECK(x) if(NULL == x) {err = ESP_FAIL; goto exit;}

static const char TAG[] = "route/v1/nvs";

/**
 * Get namespace and key from URI
 */
static uint8_t get_namespace_key_from_uri(char *namespace, char *key, const httpd_req_t *req)
{
    assert(namespace);

    uint8_t res = 0;
    const char *uri = req->uri;

    ESP_LOGD(TAG, "Parsing %s", uri);

    uri += strlen(PROJECT_ROUTE_V1_NVS);

    if( *uri == '\0' ) {
        /* parsed /api/v1/nvs - no namespace or key to parse */
        return res;
    }
    if(uri[0] != '/'){
        // Sanity check
        res |= PARSE_ERROR;
        return res;
    }
    uri++;
    if( *uri == '\0' ) {
        /* parsed /api/v1/nvs/ - no namespace or key to parse */
        return res;
    }

    /* URI should now be one of:
     *     1. Pointing to "namespace"
     *     2. Pointing to "namespace/key"
     */
    res |= PARSE_NAMESPACE;

    char *first_sep = strchr(uri, '/');
    if(NULL == first_sep){
        /* parsed /api/v1/nvs/some_namespace */
        if(strlen(uri) >= NAMESPACE_MAX) {
            ESP_LOGE(TAG, "Namespace \"%s\" too long (must be <%d char)", uri, NAMESPACE_MAX);
            res |= PARSE_ERROR;
            return res;
        }
        strcpy(namespace, uri);
        return res;
    }

    {
        size_t len = first_sep - uri;
        if(len >= NAMESPACE_MAX) {
            ESP_LOGE(TAG, "Namespace \"%s\" too long (must be <%d char)", uri, NAMESPACE_MAX);
            res |= PARSE_ERROR;
            return res;
        }
        memcpy(namespace, uri, len);
        namespace[len] = '\0';
        uri = first_sep + 1;  // skip over the '/'
    }

    if(*uri == '\0') {
        return res;
    }

    res |= PARSE_KEY;

    char *second_sep = strchr(uri, '/');
    if(NULL == second_sep){
        /* parsed /api/v1/nvs/some_namespace/some_key */
        if(strlen(uri) >= KEY_MAX) {
            ESP_LOGE(TAG, "key \"%s\" too long (must be <%d char)", uri, KEY_MAX);
            res |= PARSE_ERROR;
            return res;
        }
        if(key) {
            strcpy(key, uri);
        }
        return res;
    }
    {
        size_t len = second_sep - uri;
        if(len >= KEY_MAX) {
            ESP_LOGE(TAG, "key \"%s\" too long (must be <%d char)", uri, KEY_MAX);
            res |= PARSE_ERROR;
            return res;
        }
        if(key) {
            memcpy(key, uri, len);
            key[len] = '\0';
        }
    }

    return res;
}

static const char *nvs_type_to_str(nvs_type_t type) {
    switch(type) {
        case NVS_TYPE_U8: return "uint8";
        case NVS_TYPE_I8: return "int8";
        case NVS_TYPE_U16: return "uint16";
        case NVS_TYPE_I16: return "int16";
        case NVS_TYPE_U32: return "uint32";
        case NVS_TYPE_I32: return "int32";
        case NVS_TYPE_U64: return "uint64";
        case NVS_TYPE_I64: return "int64";
        case NVS_TYPE_STR: return "string";
        case NVS_TYPE_BLOB: return "binary";
        default: return "UNKNOWN";
    }
}

static size_t nvs_type_to_size(nvs_type_t type) {
    switch(type) {
        case NVS_TYPE_U8: return 1;
        case NVS_TYPE_I8: return 1;
        case NVS_TYPE_U16: return 2;
        case NVS_TYPE_I16: return 2;
        case NVS_TYPE_U32: return 4;
        case NVS_TYPE_I32: return 4;
        case NVS_TYPE_U64: return 8;
        case NVS_TYPE_I64: return 8;
        default: return 0;
    }
}

static esp_err_t nvs_type_set_number(nvs_handle_t h, nvs_type_t type, const char *key, double val) {
    esp_err_t err = ESP_FAIL;
    switch(type) {
        case NVS_TYPE_U8:  err = nvs_set_u8( h, key, (uint8_t) val); break;
        case NVS_TYPE_I8:  err = nvs_set_i8( h, key, (int8_t)  val); break;
        case NVS_TYPE_U16: err = nvs_set_u16(h, key, (uint16_t)val); break;
        case NVS_TYPE_I16: err = nvs_set_i16(h, key, (int16_t) val); break;
        case NVS_TYPE_U32: err = nvs_set_u32(h, key, (uint32_t)val); break;
        case NVS_TYPE_I32: err = nvs_set_i32(h, key, (int32_t) val); break;
        case NVS_TYPE_U64: err = nvs_set_u64(h, key, (uint64_t)val); break;
        case NVS_TYPE_I64: err = nvs_set_i64(h, key, (int64_t) val); break;
        default:
            break;
    }
    return err;
}


/**
 * @brief Reads and converts the value to string
 * @param[out] buf Output buffer to place string into.
 * @param[in] len Length of output buffer
 * @param[in] namespace 
 * @param[in] key
 * @return The full length (in bytes) of the data. Returns a negative value on error.
 */
static int nvs_as_str(char *buf, size_t len, const char *namespace, const char *key, nvs_type_t type)
{
    assert( len > 21 );  // So we don't have to error check number conversions
    int outlen = -1;
    esp_err_t err;
    nvs_handle_t h = 0;

    err = nvs_open(namespace, NVS_READONLY, &h);
    if(ESP_OK != err) {
        ESP_LOGE(TAG, "Couldn't open namespace %s", namespace);
        goto exit;
    }

    outlen = nvs_type_to_size(type);

#define NVS_CHECK(x) do{ if(ESP_OK != x) { outlen = -1; goto exit;} }while(0)
    switch(type) {
        case NVS_TYPE_U8: {
            uint8_t val;
            NVS_CHECK(nvs_get_u8(h, key, &val));
            itoa(val, buf, 10);
            break;
        }
        case NVS_TYPE_I8: {
            int8_t val;
            NVS_CHECK(nvs_get_i8(h, key, &val));
            itoa(val, buf, 10);
            break;
        }
        case NVS_TYPE_U16: {
            uint16_t val;
            NVS_CHECK(nvs_get_u16(h, key, &val));
            itoa(val, buf, 10);
            break;
        }
        case NVS_TYPE_I16: {
            int16_t val;
            NVS_CHECK(nvs_get_i16(h, key, &val));
            itoa(val, buf, 10);
            break;
        }
        case NVS_TYPE_U32: {
            uint32_t val;
            NVS_CHECK(nvs_get_u32(h, key, &val));
            sprintf(buf, "%d", val);
            break;
        }
        case NVS_TYPE_I32: {
            int32_t val;
            NVS_CHECK(nvs_get_i32(h, key, &val));
            itoa(val, buf, 10);
            break;
        }
        case NVS_TYPE_U64: {
            uint64_t val;
            NVS_CHECK(nvs_get_u64(h, key, &val));
            sprintf(buf, "%lld", val);
            break;
        }
        case NVS_TYPE_I64: {
            int64_t val;
            NVS_CHECK(nvs_get_i64(h, key, &val));
            sprintf(buf, "%lld", val);
            break;
        }
        case NVS_TYPE_STR: {
            NVS_CHECK(nvs_get_str(h, key, NULL, (size_t *)&outlen));
            if(outlen > len) {
                strlcpy(buf, "&lt;exceeds buffer length&gt;", len);
                goto exit;
            }
            NVS_CHECK(nvs_get_str(h, key, buf, (size_t *)&outlen));
            break;
        }
        case NVS_TYPE_BLOB: {
            uint8_t *bin;
            NVS_CHECK(nvs_get_blob(h, key, NULL, (size_t *)&outlen));
            if(outlen > (len-1) / 2) {
                strlcpy(buf, "&lt;exceeds buffer length&gt;", len);
                goto exit;
            }
            if((bin = malloc(outlen)) == NULL) goto exit;
            if(ESP_OK != nvs_get_blob(h, key, bin, (size_t *)&outlen)) {
                free(bin);
                goto exit;
            }
            sodium_bin2hex(buf, len, bin, outlen);
            free(bin);
            break;
        }
        default: 
            outlen = -2;
            goto exit;
    }
#undef NVS_CHECK

exit:
    if( h ) nvs_close(h);
    ESP_LOGD(TAG, "outlen: %d", outlen);
    return outlen;
}


/**
 * @brief GET handler that responds with json data for a namespace/key
 */
static esp_err_t nvs_namespace_key_get_handler(httpd_req_t *req, const char *namespace, const char *key) {
    esp_err_t err = ESP_FAIL;
    nvs_entry_info_t info;
    const char *msg = NULL;
    cJSON *root = NULL;
    nvs_handle_t h = 0;

    nvs_iterator_t it = nvs_entry_find(NVS_DEFAULT_PART_NAME, namespace, NVS_TYPE_ANY);
    while (it != NULL) {
        nvs_entry_info(it, &info);
        if(0 == strcmp(info.key, key)) {
            break;
        }
        it = nvs_entry_next(it);
    }
    if(it == NULL) {
        /* Key wasn't found */
        ESP_LOGE(TAG, "Couldn't find %s in namespace %s", key, namespace);
        goto exit;
    }

    root = cJSON_CreateObject();
    CJSON_CHECK(cJSON_AddStringToObject(root, "namespace", namespace));
    CJSON_CHECK(cJSON_AddStringToObject(root, "key", key));
    CJSON_CHECK(cJSON_AddStringToObject(root, "dtype", nvs_type_to_str(info.type)));

    if(ESP_OK != (err = nvs_open(namespace, NVS_READONLY, &h))) {
        ESP_LOGE(TAG, "Couldn't open namespace %s", namespace);
        goto exit;
    }

    size_t dsize;
    switch(info.type) {
        case NVS_TYPE_U8:{
            uint8_t val;
            if(ESP_OK != (err = nvs_get_u8(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 1;
            break;
        }
        case NVS_TYPE_I8: {
            int8_t val;
            if(ESP_OK != (err = nvs_get_i8(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 1;
            break;
        }
        case NVS_TYPE_U16: {
            uint16_t val;
            if(ESP_OK != (err = nvs_get_u16(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 2;
            break;
        }
        case NVS_TYPE_I16: {
            int16_t val;
            if(ESP_OK != (err = nvs_get_i16(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 2;
            break;
        }
        case NVS_TYPE_U32: {
            uint32_t val;
            if(ESP_OK != (err = nvs_get_u32(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 4;
            break;
        }
        case NVS_TYPE_I32: {
            int32_t val;
            if(ESP_OK != (err = nvs_get_i32(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 4;
            break;
        }
        case NVS_TYPE_U64: {
            uint64_t val;
            if(ESP_OK != (err = nvs_get_u64(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 8;
            break;
        }
        case NVS_TYPE_I64: {
            int64_t val;
            if(ESP_OK != (err = nvs_get_i64(h, key, &val))) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 8;
            break;
        }
        case NVS_TYPE_STR: {
            cJSON *obj;
            char *val;
            if(ESP_OK != (err = nvs_get_str(h, key, NULL, &dsize))) goto exit;
            if(NULL == (val = malloc(dsize))) {
                ESP_LOGE(TAG, "OOM");
                err = ESP_FAIL;
                goto exit;
            }
            if(ESP_OK != (err = nvs_get_str(h, key, val, &dsize))) goto exit;
            obj = cJSON_AddStringToObject(root, "value", val);
            free(val);
            if(NULL == obj) {
                err = ESP_FAIL;
                goto exit;
            }
            break;
        }
        case NVS_TYPE_BLOB: {
            cJSON *obj;
            char *hex;
            uint8_t *val;
            if(ESP_OK != (err = nvs_get_blob(h, key, NULL, &dsize))) goto exit;
            if(NULL == (val = malloc(dsize))) {
                ESP_LOGE(TAG, "OOM");
                err = ESP_FAIL;
                goto exit;
            }
            if(ESP_OK != (err = nvs_get_blob(h, key, val, &dsize))) goto exit;

            size_t hex_len = dsize * 2 + 1;
            if(NULL == (hex = malloc(hex_len))) {
                ESP_LOGE(TAG, "OOM");
                err = ESP_FAIL;
                free(val);
                goto exit;
            }

            sodium_bin2hex(hex, hex_len, val, dsize);
            obj = cJSON_AddStringToObject(root, "value", hex);

            free(val);
            free(hex);
            if(NULL == obj) {
                err = ESP_FAIL;
                goto exit;
            }
            break;
        }
        default:
            // Do Nothing
            break;
    }

    cJSON_AddNumberToObject(root, "size", dsize);

    msg = cJSON_Print(root);
    if(msg) {
        httpd_resp_sendstr(req, msg);
        err = ESP_OK;
    }

exit:
    if( msg ) free(msg);
    if( root ) cJSON_Delete(root);
    if( h ) nvs_close(h);
    return err;
}

/**
 * @brief Render the html table with NVS contents.
 * @param[in] req
 * @param[in] namespace Namespace to iterate over. Set to NULL to iterate over
 * all namespaces
 */
static esp_err_t nvs_namespace_get_handler(httpd_req_t *req, const char *namespace)
{
    esp_err_t err = ESP_FAIL;
    bool serve_html = detect_if_browser(req);

    if( serve_html ){
        /* Send over jquery */
        /* Send file-list table definition and column labels */
        httpd_resp_sendstr_chunk(req,
            "<table id=\"nvs\" class=\"fixed\" border=\"1\">"
            "<col width=\"400px\" />"
            "<col width=\"400px\" />"
            "<col width=\"400px\" />"
            "<col width=\"400px\" />"
            "<col width=\"400px\" />"
            "<thead><tr>"
            "<th>Namespace</th>"
            "<th>Key</th>"
            "<th>Value</th>"
            "<th>Dtype</th>"
            "<th>Size (Bytes)</th>"
            "</thead>"
            "<tbody>");
    }
    else{
        /* Send JSON Meta */
        httpd_resp_sendstr_chunk(req, "{\"contents\":[");
    }

    nvs_iterator_t it = nvs_entry_find(NVS_DEFAULT_PART_NAME, namespace, NVS_TYPE_ANY);
    bool first_iter = true;
    while (it != NULL) {
        int len;
        char size_buf[16] = {0};
        char value_buf[256] = {0};
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        it = nvs_entry_next(it);

        ESP_LOGD(TAG, "key '%s', type '%d'", info.key, info.type);

        len = nvs_as_str(value_buf, sizeof(value_buf), info.namespace_name, info.key, info.type);
        if(len < 0) {
            ESP_LOGE(TAG, "Unhandled error");
            continue;
        }
        itoa(len, size_buf, 10);

        if(serve_html) {
            httpd_resp_sendstr_chunk(req, "<tr><td>");
            httpd_resp_sendstr_chunk(req, info.namespace_name);
            httpd_resp_sendstr_chunk(req, "</td><td>");
            httpd_resp_sendstr_chunk(req, info.key);
            httpd_resp_sendstr_chunk(req, "</td><td><input type='text' name='");
            httpd_resp_sendstr_chunk(req, info.key);
            httpd_resp_sendstr_chunk(req, "' data-namespace='");
            httpd_resp_sendstr_chunk(req, info.namespace_name);
            httpd_resp_sendstr_chunk(req, "' value='");
            httpd_resp_sendstr_chunk(req, value_buf);
            httpd_resp_sendstr_chunk(req, "' /></td><td>");
            httpd_resp_sendstr_chunk(req, nvs_type_to_str(info.type));
            httpd_resp_sendstr_chunk(req, "</td><td>");
            httpd_resp_sendstr_chunk(req, size_buf);
            httpd_resp_sendstr_chunk(req, "</td></tr>\n");
        }
        else {
            if(!first_iter) {
                httpd_resp_sendstr_chunk(req, ",");
            }
            httpd_resp_sendstr_chunk(req, "{\"namespace\":\"");
            httpd_resp_sendstr_chunk(req, info.namespace_name);
            httpd_resp_sendstr_chunk(req, "\",\"key\":\"");
            httpd_resp_sendstr_chunk(req, info.key);
            httpd_resp_sendstr_chunk(req, "\",\"value\":\"");
            httpd_resp_sendstr_chunk(req, value_buf);
            httpd_resp_sendstr_chunk(req, "\",\"dtype\":\"");
            httpd_resp_sendstr_chunk(req, nvs_type_to_str(info.type));
            httpd_resp_sendstr_chunk(req, "\",\"size\":");
            httpd_resp_sendstr_chunk(req, size_buf);
            httpd_resp_sendstr_chunk(req, "}");
        }
        first_iter = false;
    }

    if(serve_html){
        /* Finish the file list table */
        httpd_resp_sendstr_chunk(req, "</tbody></table>");

        HTTP_SEND_JS(req, api_v1_nvs);

        /* Send remaining chunk of HTML file to complete it */
        httpd_resp_sendstr_chunk(req, "</body>");

        httpd_resp_sendstr_chunk(req, "</html>");
    }
    else{
        httpd_resp_sendstr_chunk(req, "]}");
    }

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

    err = ESP_OK;

    return err;
}

esp_err_t nvs_post_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    uint8_t res;
    char namespace[NAMESPACE_MAX] = {0};
    nvs_handle_t h = 0;
    nvs_iterator_t it;
    cJSON *root = NULL;
    cJSON *elem;

    res = get_namespace_key_from_uri(namespace, NULL, req);
    if(res & PARSE_ERROR) {
        ESP_LOGE(TAG, "Failed to parse namespace");
        goto exit;
    }
    if(res & PARSE_KEY) {
        ESP_LOGE(TAG, "Don't supply key in URI");
        goto exit;
    }
    if(!(res & PARSE_NAMESPACE)) {
        ESP_LOGE(TAG, "Missing required namespace");
        goto exit;
    }

    if(ESP_OK != parse_post_request(&root, req)) goto exit;

    /* Open the namespace */
    err = nvs_open(namespace, NVS_READWRITE, &h);
    if(ESP_OK != err) goto exit;

    /* Iterate over all provided keys */
    cJSON_ArrayForEach(elem, root) {
        nvs_entry_info_t info;
        char *key = elem->string;
        bool found = false;

        cJSON *item;
        item = cJSON_GetObjectItemCaseSensitive(root, elem->string);
        if(NULL == item) goto exit;

        it = nvs_entry_find(NVS_DEFAULT_PART_NAME, namespace, NVS_TYPE_ANY);

        /* Get the info for this particular namespace/key */
        while (it != NULL) {
            nvs_entry_info(it, &info);
            if(0 == strcmp(info.key, key)) {
                found = true;
                break;
            }
            it = nvs_entry_next(it);
        }
        nvs_release_iterator(it);
        it = NULL;
        if(!found) goto exit;

        /* Update the found key */
        if(info.type == NVS_TYPE_STR) {
            if(! cJSON_IsString(item)) {
                ESP_LOGE(TAG, "Value for key \"%s\" must be a string", key);
                err = ESP_FAIL;
                goto exit;
            }
            if(ESP_OK == (err = nvs_set_str(h, key, item->valuestring))){
                ESP_LOGI(TAG, "Saved string \"%s\" to %s/%s", item->valuestring, namespace, key);
            }
        }
        else if(info.type == NVS_TYPE_BLOB) {
            if(! cJSON_IsString(item)) {
                ESP_LOGE(TAG, "Value for key \"%s\" must be a hex string", key);
                err = ESP_FAIL;
                goto exit;
            }

            size_t bin_actual_len;
            size_t bin_buf_len = strlen(item->valuestring) / 2;
            uint8_t *bin;
            if(NULL == (bin = malloc(bin_buf_len))) {
                ESP_LOGE(TAG, "OOM");
                goto exit;
            }

            int res;
            res = sodium_hex2bin(bin, bin_buf_len, 
                    item->valuestring, strlen(item->valuestring),
                    NULL, &bin_actual_len, NULL);
            if(res != 0) {
                ESP_LOGE(TAG, "Failed to convert hexstring to bin");
                free(bin);
                goto exit;
            }
            if( ESP_OK == (err = nvs_set_blob(h, key, bin, bin_actual_len))) {
                ESP_LOGI(TAG, "Saved blob \"%s\" to %s/%s", item->valuestring, namespace, key);
            }
            free(bin);
        }
        else {
            double val;
            if(cJSON_IsNumber(item)) {
                val = item->valuedouble;
                ESP_LOGE(TAG, "Value for key \"%s\" must be a number", key);
                err = ESP_FAIL;
                goto exit;
            }
            else if(cJSON_IsString(item)) {
                errno = 0;
                val = strtod(item->valuestring, NULL);
                if(errno != 0) {
                    ESP_LOGE(TAG, "Could not convert value to double");
                    goto exit;
                }
            }
            else {
                ESP_LOGI(TAG, "Unhandled datatype");
                goto exit;
            }
            if( ESP_OK == (err = nvs_type_set_number(h, info.type, key, val))) {
                ESP_LOGI(TAG, "Saved number %lf to %s/%s", val, namespace, key);
            }
        }

        if(ESP_OK != err) goto exit;
    }

    err = ESP_OK;
    
exit:
    httpd_resp_send_chunk(req, NULL, 0);
    if(root) cJSON_Delete(root);
    if( h ) nvs_close(h);
    nvs_release_iterator(it);
    if(ESP_OK != err) ESP_LOGE(TAG, "nvs post failed");
    return err;
}

esp_err_t nvs_get_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    char namespace[NAMESPACE_MAX] = {0};
    char key[KEY_MAX] = {0};
    uint8_t res;

    res = get_namespace_key_from_uri(namespace, key, req);
    if(res & PARSE_ERROR) {
        goto exit;
    }

    if(res & PARSE_NAMESPACE && res & PARSE_KEY) {
        /* Directly return the value in form:
         * {
         *     "namespace": "<namespace>",
         *     "key": "<key>",
         *     "value": <value>,
         *     "dtype": <str>,
         * }
         */
        err = nvs_namespace_key_get_handler(req, namespace, key);
        goto exit;
    }

    if(res & PARSE_NAMESPACE) {
        /* Display key/values within this namespace */
        err = nvs_namespace_get_handler(req, namespace);
        goto exit;
    }
    else {
        /* Display all key/values across all namespaces */
        err = nvs_namespace_get_handler(req, NULL);
        goto exit;
    }

exit:
    return err;
}

