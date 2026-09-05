#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp32-dht11.h"
#include "led_strip.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "SMART_GREENHOUSE";

// ==========================================
// 1. 핀 배정
// ==========================================
#define CONFIG_DHT11_PIN GPIO_NUM_9
#define ADC_UNIT ADC_UNIT_1
#define ADC_CHAN ADC_CHANNEL_2
#define BLINK_GPIO GPIO_NUM_8

// 모터(L9110s) 제어용 GPIO 핀 2개를 하드웨어 연결에 맞게 설정
#define MOTOR_PIN_A GPIO_NUM_19 //A모터의 IA핀
#define MOTOR_PIN_B GPIO_NUM_18 //A모터의 IB핀

// 릴레이 제어용 GPIO 핀을 설정
#define RELAY_PIN GPIO_NUM_23 

static led_strip_handle_t led_strip;

// ==========================================
// 2. 초기화 함수들 
// ==========================================
static void configure_led(void) {
    ESP_LOGI(TAG, "Configuring LED Strip");
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "LED strip configured successfully");
}

static void configure_actuators(void) {
    // 모터 핀(A, B), 릴레이 핀 디지털 출력 모드로 초기화
    gpio_reset_pin(MOTOR_PIN_A);
    gpio_reset_pin(MOTOR_PIN_B);
    gpio_reset_pin(RELAY_PIN);

    gpio_set_direction(MOTOR_PIN_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_PIN_B, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);  
    
}

// ==========================================
// 3. 시스템 제어 함수 
// ==========================================
static void control_system(float temp, float hum, int light) {
    // 우선 순위에 따른 제어로직 구현

    // 1. 환기 시스템(모터) 제어 로직
    if (/* 온도 28도 이상 또는 습도 70% 이상 */temp>=28 || hum>=70) {
        // 모터 ON(회전)
        gpio_set_level(MOTOR_PIN_A,1);
        gpio_set_level(MOTOR_PIN_B,0);
    } else {
        // 모터 OFF(정지)
        gpio_set_level(MOTOR_PIN_A, 0);
        gpio_set_level(MOTOR_PIN_B, 0);
    }

    // 2. 생장 조명(릴레이) 제어 로직
    if (/* 조도 1500 미만 */light<1500) {
        // 릴레이 ON
        gpio_set_level(RELAY_PIN, 1);
    } else {
        // 릴레이 OFF
        gpio_set_level(RELAY_PIN, 0);
    }

    // 3. 상태 표시장치(내장 RGB LED) 제어우선순위 로직
    if (/* 1순위: 온습도 경고 모드 조건 */temp>=28 || hum>=70) {
        // Red 100% 점등: 
        led_strip_set_pixel(led_strip, 0, 255, 0, 0);
    } else if (/* 2순위: 야간 조명 작동 모드 조건 */light<1500) {
        // White 100% 점등: 
        led_strip_set_pixel(led_strip, 0, 255, 255, 255);
    } else {
        // 3순위: 정상 대기 모드 (Green)
        led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    }
    
    led_strip_refresh(led_strip);
}

// ==========================================
// 4. 메인 루프 
// ==========================================
void app_main() {
    //1. 센서 구조체 및 ADC 핸들 선언
    dht11_t dht11_sensor;
    dht11_sensor.dht11_pin = CONFIG_DHT11_PIN;

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

    //2. 하드웨어 초기화 호출
    configure_led();
    configure_actuators();

    int adc_raw = 0;
    int loop_counter = 0; // 주기 측정을 위한 카운터 변수

    //3. 데이터 수집 및 제어 
    while(1) {
        // [수집 1] 조도 센서 측정 (매 1초마다, 즉 매 루프마다 실행)
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHAN, &adc_raw));
        printf("[Light Intensity]> %d \n", adc_raw);

        // [수집 2] 온습도 센서 측정 (매 2초마다, 즉 카운터가 짝수일 때만 실행)
        if (loop_counter % 2 == 0) {
            if(!dht11_read(&dht11_sensor, 5)) {
                printf("[Temperature]> %.2f \n", dht11_sensor.temperature);
                printf("[Humidity]> %.2f \n", dht11_sensor.humidity);
            }
        }

        // 수집된 데이터를 바탕으로 제어 로직 호출
        control_system(dht11_sensor.temperature, dht11_sensor.humidity, adc_raw);

        // 루프 카운터 증가 및 1초 대기
        loop_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    } 
}