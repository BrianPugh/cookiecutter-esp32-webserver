#include "route/v1/nvs.h"
#include "nvs.h"
#include <sys/param.h>

// Including NULL-terminator
#define NAMESPACE_MAX 16
#define KEY_MAX 16

// Bitfield of possible outcomes while parsing URI
#define PARSE_NAMESPACE ( 1 << 0 )  // Namespace was parsed
#define PARSE_KEY       ( 1 << 1 )  // Key was parsed
#define PARSE_ERROR       ( 1 << 7 ) // Error during parsing

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
    char *p;

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
    // TODO
    esp_err_t err = ESP_FAIL;
    char namespace[NAMESPACE_MAX] = {0};
    char key[KEY_MAX] = {0};
    extern const unsigned char script_start[] asm("_binary_api_v1_nvs_html_start");
    extern const unsigned char script_end[]   asm("_binary_api_v1_nvs_html_end");

    {
        uint8_t res;
        res = get_namespace_key_from_uri(namespace, key, req);
        if(res & PARSE_ERROR) {
            goto exit;
        }
        if(res & PARSE_NAMESPACE) {
            ESP_LOGI(TAG, "Parsed namespace: \"%s\"", namespace);
        }
        if(res & PARSE_KEY) {
            ESP_LOGI(TAG, "Parsed key: \"%s\"", key);
        }
    }

exit:
    return err;
}

