#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "led_strip.h"

#define ADC_UNIT        ADC_UNIT_1
#define ADC_CHAN        ADC_CHANNEL_2
#define LED_GPIO_PIN    8

static led_strip_handle_t led_strip;

void configure_led(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_PIN,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void app_main(void) {
    configure_led();

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHAN, &config));

    int adc_raw;
    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN, &adc_raw));
        
        if (adc_raw > 2200) { 
            printf("[Light Sensor] ADC: %d --> Status: Dark (Red LED)\n", adc_raw);
            led_strip_set_pixel(led_strip, 0, 255, 0, 0); 
        } 
        else if (adc_raw > 900) { 
            printf("[Light Sensor] ADC: %d --> Status: Dim (Blue LED)\n", adc_raw);
            led_strip_set_pixel(led_strip, 0, 0, 0, 255); 
        } 
        else {
            printf("[Light Sensor] ADC: %d --> Status: Bright (Green LED)\n", adc_raw);
            led_strip_set_pixel(led_strip, 0, 0, 255, 0); 
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
    } // while문 닫기
} // app_main 닫기