#ifndef PROJECT_FILESYSTEM_H__
#define PROJECT_FILESYSTEM_H__


#include "esp_err.h"


#define CONFIG_PROJECT_FS_MOUNT_POINT "/fs"

#define MAX_FILE_SIZE   (500*1024) // 500 KB
#define MAX_FILE_SIZE_STR "500KB"
#define MAX_FILE_PATH 256

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/**
 * @brief initializes filesystem configured via menuconfig
 */
esp_err_t init_fs(void);


/**
 * @brief Recursively create parent directories for a given path
 * @param[in] path Path to a file/directory to create paths up to
 * @param[in] isfile If `true`, do not create the last token.
 * @returns 0 on success. On failure, a partial directory structure may have
 * been created.
 */
esp_err_t mkdir_p(const char *path, bool isfile);


/**
 * @brief modifies path in place to remove repeated '/'
 */
char *trim_separators(char *path);


/**
 * @brief Delete a file/folder and all its contents
 *
 * Based on https://stackoverflow.com/a/42596507
 */
int rm_rf(const char path[]);

#endif
