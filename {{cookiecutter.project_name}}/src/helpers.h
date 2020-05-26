#ifndef PROJECT_HELPERS_H__
#define PROJECT_HELPERS_H__

/**
 * Gets a stored string or sets it and returns the default value if not present.
 *
 * Caller must free the returns string.
 *
 * @param[in] namespace NVS Namespace to use
 * @param[in] key Key to get value of.
 * @param[in] def Default value to set-and-return if namespace/key doesn't exist.
 * @return string NULL on error
 */
char *nvs_get_str_default(const char *namespace, const char *key, const char *def);

#endif
