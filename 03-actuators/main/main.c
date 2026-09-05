#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"    // RGB LED 제어용
#include "driver/adc.h"     // 조도 센서 제어용
#include "esp32-dht11.h"

// 핀 설정 
#define CONFIG_DHT11_PIN     GPIO_NUM_9
#define CONFIG_LDR_CHANNEL   ADC1_CHANNEL_0  // GPIO 0번 가정
#define RGB_RED_PIN          GPIO_NUM_3
#define RGB_GREEN_PIN        GPIO_NUM_4
#define RGB_BLUE_PIN         GPIO_NUM_5

// 센서 데이터 저장용 전역 변수
float current_temp = 25.0; 
int current_ldr = 2000;

// LED 제어 함수 
void update_led_logic() {
    int r = 0, g = 0, b = 0;

    // 우선순위 1- 온도가 10도 이하 -> RGB 빨강 100%
    if (current_temp <= 10.0) {
        r = 255; g = 0; b = 0;
    } 
    // 우선순위 2- 어두우면 (조도 1500 미만) -> RGB 흰색 100%
    else if (current_ldr < 1500) {
        r = 255; g = 255; b = 255;
    }
    // 그 외 상황은 LED 끄기
    else {
        r = 0; g = 0; b = 0;
    }

    // PWM 값 적용
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, r);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, g);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, b);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

// 하드웨어 초기화 (PWM, ADC)
void init_hardware() {
    //  PWM(LEDC) 설정
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    int pins[3] = {RGB_RED_PIN, RGB_GREEN_PIN, RGB_BLUE_PIN};
    for(int i=0; i<3; i++) {
        ledc_channel_config_t chan_conf = {
            .gpio_num = pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = i,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0
        };
        ledc_channel_config(&chan_conf);
    }

    // ADC(조도 센서) 설정
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(CONFIG_LDR_CHANNEL, ADC_ATTEN_DB_11);
}

void app_main() {
    init_hardware();

    dht11_t dht11_sensor;
    dht11_sensor.dht11_pin = CONFIG_DHT11_PIN;

    int second_counter = 0;

    while(1) {
        // 조도 센서 값 읽기 (1초 간격)
        current_ldr = adc1_get_raw(CONFIG_LDR_CHANNEL);
        printf("[1s] LDR: %d\n", current_ldr);

        //  온습도 센서 값 읽기 (2초 간격)
        if (second_counter % 2 == 0) {
            if(!dht11_read(&dht11_sensor, 5)) {
                current_temp = dht11_sensor.temperature;
                printf("[2s] Temp: %.2f, Humi: %.2f\n", current_temp, dht11_sensor.humidity);
            }
        }

        //  LED 판단 및 갱신
        update_led_logic();

        second_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1초 대기
    }
}
