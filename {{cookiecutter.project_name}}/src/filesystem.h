#ifndef PROJECT_FILESYSTEM_H__
#define PROJECT_FILESYSTEM_H__


#include "esp_err.h"


#define CONFIG_PROJECT_FS_MOUNT_POINT "/fs"

/**
 * @brief initializes filesystem configured via menuconfig
 */
esp_err_t init_fs(void);

#endif
