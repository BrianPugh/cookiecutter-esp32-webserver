#include "route/v1/nvs.h"
#include "nvs.h"
#include "nvs_flash.h"
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
    assert(key);

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
        strcpy(key, uri);
        return res;
    }
    {
        size_t len = second_sep - uri;
        if(len >= KEY_MAX) {
            ESP_LOGE(TAG, "key \"%s\" too long (must be <%d char)", uri, KEY_MAX);
            res |= PARSE_ERROR;
            return res;
        }
        memcpy(key, uri, len);
        key[len] = '\0';
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
            //TODO
            outlen = 0;
        }
        case NVS_TYPE_BLOB: {
            //TODO
            outlen = 0;
        }
        default: 
            outlen = -2;
            goto exit;
    }
#undef NVS_CHECK

exit:
    if( h ) nvs_close(h);
    return outlen;
}

static esp_err_t nvs_namespace_key_get_handler(httpd_req_t *req, const char *namespace, const char *key) {
    esp_err_t err = ESP_FAIL;
    // Find the key within the namespace
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
    cJSON_AddStringToObject(root, "namespace", namespace);
    cJSON_AddStringToObject(root, "key", key);
    cJSON_AddStringToObject(root, "dtype", nvs_type_to_str(info.type));

    err = nvs_open(namespace, NVS_READONLY, &h);
    if(ESP_OK != err) {
        ESP_LOGE(TAG, "Couldn't open namespace %s", namespace);
        goto exit;
    }

    size_t dsize;
    switch(info.type) {
        case NVS_TYPE_U8:{
            uint8_t val;
            err = nvs_get_u8(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 1;
            break;
        }
        case NVS_TYPE_I8: {
            int8_t val;
            err = nvs_get_i8(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 1;
            break;
        }
        case NVS_TYPE_U16: {
            uint16_t val;
            err = nvs_get_u16(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 2;
            break;
        }
        case NVS_TYPE_I16: {
            int16_t val;
            err = nvs_get_i16(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 2;
            break;
        }
        case NVS_TYPE_U32: {
            uint32_t val;
            err = nvs_get_u32(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 4;
            break;
        }
        case NVS_TYPE_I32: {
            int32_t val;
            err = nvs_get_i32(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 4;
            break;
        }
        case NVS_TYPE_U64: {
            uint64_t val;
            err = nvs_get_u64(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 8;
            break;
        }
        case NVS_TYPE_I64: {
            int64_t val;
            err = nvs_get_i64(h, key, &val);
            if(ESP_OK != err) goto exit;
            CJSON_CHECK(cJSON_AddNumberToObject(root, "value", val));
            dsize = 8;
            break;
        }
        case NVS_TYPE_STR: {
            char *val;
            err = nvs_get_str(h, key, NULL, &dsize);
            if(ESP_OK != err) goto exit;
            val = malloc(dsize);
            if(NULL == val) {
                ESP_LOGE(TAG, "OOM");
                err = ESP_FAIL;
                goto exit;
            }
            err = nvs_get_str(h, key, val, &dsize);
            if(ESP_OK != err) goto exit;

            cJSON *obj;
            obj = cJSON_AddStringToObject(root, "value", val);
            free(val);
            if(NULL == obj) {
                err = ESP_FAIL;
                goto exit;
            }
            break;
        }
        case NVS_TYPE_BLOB: {
            uint8_t *val;
            err = nvs_get_blob(h, key, NULL, &dsize);
            if(ESP_OK != err) goto exit;
            // TODO: parse and convert value
            break;
        }
        default:
            // Do Nothing
            break;
    }

    cJSON_AddNumberToObject(root, "size", dsize);

    msg = cJSON_Print(root);
    httpd_resp_sendstr(req, msg);
    err = ESP_OK;

exit:
    if( msg ) free(msg);
    if( root ) cJSON_Delete(root);
    if( h ) nvs_close(h);
    return err;
}

static esp_err_t nvs_namespace_get_handler(httpd_req_t *req, const char *namespace)
{
    esp_err_t err = ESP_FAIL;
    extern const unsigned char script_start[] asm("_binary_api_v1_nvs_html_start");
    extern const unsigned char script_end[]   asm("_binary_api_v1_nvs_html_end");
    const size_t script_size = script_end - script_start;

    //httpd_resp_send_chunk(req, (const char *)script_start, script_size);
    
    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
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

    nvs_iterator_t it = nvs_entry_find(NVS_DEFAULT_PART_NAME, namespace, NVS_TYPE_ANY);
    while (it != NULL) {
        size_t len;
        char size_buf[16] = {0};
        char value_buf[256] = {0};
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        it = nvs_entry_next(it);
        ESP_LOGI(TAG, "key '%s', type '%d'", info.key, info.type);

        httpd_resp_sendstr_chunk(req, "<tr><td>");
        httpd_resp_sendstr_chunk(req, namespace);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, info.key);
        httpd_resp_sendstr_chunk(req, "</td><td>");

        /* Value */
        len = nvs_as_str(value_buf, sizeof(value_buf), namespace, info.key, info.type);
        if(len < 0) {
            // TODO: error handling
        }
        httpd_resp_sendstr_chunk(req, value_buf);

        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, nvs_type_to_str(info.type));
        httpd_resp_sendstr_chunk(req, "</td><td>");
        if(info.type != NVS_TYPE_STR || info.type != NVS_TYPE_BLOB) {
            len = nvs_type_to_size(info.type);
        }
        itoa(len, size_buf, 10);
        httpd_resp_sendstr_chunk(req, size_buf);
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);


    err = ESP_OK;

    return err;
}

static esp_err_t nvs_root_get_handler(httpd_req_t *req)
{
    esp_err_t err = ESP_FAIL;
    // TODO
    return err;
}

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
        err = nvs_root_get_handler(req);
        goto exit;
    }

exit:
    return err;
}

