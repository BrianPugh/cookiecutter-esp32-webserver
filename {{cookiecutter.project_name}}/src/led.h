#ifndef PROJECT_LED_HAL_H__
#define PROJECT_LED_HAL_H__

#include "esp_err.h"
#include "sdkconfig.h"
#include "stdbool.h"

#define LED_INDICATOR_ON 1
#define LED_INDICATOR_OFF 0

/****
 * Setup hardware for LED.
 * Call this once in main()
 */
esp_err_t led_setup();


/**
 * Set LED Value
 */
esp_err_t led_set(bool val);

/***
 * Get LED Value
 */
bool led_get();

/****
 * Toggle LED
 */
esp_err_t led_toggle();

#endif
