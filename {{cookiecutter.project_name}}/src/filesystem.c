#include "errno.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "filesystem.h"
#include "sdkconfig.h"
#include "sdmmc_cmd.h"

#if CONFIG_PROJECT_WEB_DEPLOY_SD
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#endif

static const char TAG[] = "filesystem";


#if CONFIG_PROJECT_WEB_DEPLOY_SD

esp_err_t init_fs(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY); // CMD
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);  // D0
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);  // D1
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY); // D2
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY); // D3

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(CONFIG_PROJECT_FS_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }
    /* print card info if mount successfully */
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

#elif CONFIG_PROJECT_WEB_DEPLOY_SF

esp_err_t init_fs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = CONFIG_PROJECT_FS_MOUNT_POINT,
        .partition_label = "filesystem",
        .format_if_mount_failed = true  // Change this to false if you intend to flash specific contents to the internal filesystem
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

#else
#error "Invalid filesystem configuration"
#endif

/**
 * @brief Makes a directory if it doesn't exist
 *
 * See also: `mkdir_p`
 */
static esp_err_t mkdir_if_not_exist(const char *path) {
    struct stat sb;
    if (stat(path, &sb) == 0) {
        //Directory or file exists
        
        if(S_ISDIR(sb.st_mode)) {
            // Is a directory
            // Do Nothing
        }
        else {
            // Was a file
            return ESP_FAIL;
        }
    }
    else {
        // Directory doesn't exist, make it
        if ( 0 != mkdir(path, S_IRWXU) ) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

/**
 * @brief Recursively create parent directories for a given path
 * @param[in] path Path to a file/directory to create paths up to
 * @param[in] isfile If `true`, do not create the last token.
 * @returns 0 on success. On failure, a partial directory structure may have
 * been created.
 */
esp_err_t mkdir_p(const char *path, bool isfile)
{
    esp_err_t err = ESP_FAIL;
    char *_path = NULL;

    _path = strdup(path);
    if(NULL == _path) goto exit;

    errno = 0;
    for (char *p = _path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if(ESP_OK != mkdir_if_not_exist(_path)){
                goto exit;
            }
            *p = '/';
        }
    }   

    if( !isfile && 0 != mkdir_if_not_exist(_path)) {
        goto exit;
    }

    err = ESP_OK;

exit:
    if(_path) free(_path);
    return err;
}


/**
 * @brief modifies path in place to remove repeated '/'
 */
char *trim_separators(char *path) {
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

