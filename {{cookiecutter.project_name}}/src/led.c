#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "led.h"

/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to 
 * blink.
*/

esp_err_t led_setup()
{
    // TODO: Error handling
    gpio_pad_select_gpio(CONFIG_PROJECT_INDICATOR_LED_GPIO);
    gpio_set_direction(CONFIG_PROJECT_INDICATOR_LED_GPIO, GPIO_MODE_INPUT_OUTPUT);
    led_set(LED_INDICATOR_OFF);
    return ESP_OK;
}


esp_err_t led_set(bool val)
{
    // TODO: Error handling
    gpio_set_level(CONFIG_PROJECT_INDICATOR_LED_GPIO, val);
    return ESP_OK;
}


bool led_get()
{
    return gpio_get_level(CONFIG_PROJECT_INDICATOR_LED_GPIO);
}

esp_err_t led_toggle()
{
    return led_set(!led_get());
}
