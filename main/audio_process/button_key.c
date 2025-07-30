/**
 * @file button_key.c
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief
 * @version 0.1
 * @date 2025-07-21
 *
 * @copyright
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2021 - 2026 THL A29 Limited, a Tencent company.All rights reserved.
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "button_key.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#define TAG "BUTTON"

#define BUTTON_PIN GPIO_NUM_0  // 使用GPIO0作为按键检测引脚
#define LED_PIN    GPIO_NUM_4  // 使用GPIO4作为LED引脚

typedef enum
{
    USER_BUTTON_0 = 0,
    USER_BUTTON_MAX,
} user_button_t;

/* User defined settings */
static const ebtn_btn_param_t defaul_ebtn_param = EBTN_PARAMS_INIT(20, 0, 20, 300, 200, 500, 10);

static ebtn_btn_t btns[] = {
    EBTN_BUTTON_INIT(USER_BUTTON_0, &defaul_ebtn_param),
};


/**
 * \brief           Get input state callback
 * \param           btn: Button instance
 * \return          `1` if button active, `0` otherwise
 */
uint8_t prv_btn_get_state(struct ebtn_btn *btn)
{
    /*
     * Function will return negative number if button is pressed,
     * or zero if button is releases
     */
    return gpio_get_level(BUTTON_PIN) == 0 ? 1 : 0;
}

int button_key_init(ebtn_evt_fn event_cb)
{
   esp_err_t ret;
    // 配置GPIO参数
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),   // 设置GPIO0
        .mode         = GPIO_MODE_INPUT,        // 输入模式
        .pull_up_en   = GPIO_PULLUP_ENABLE,     // 启用内部上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用内部下拉电阻
        .intr_type    = GPIO_INTR_DISABLE       // 暂时禁用中断
    };

    // 应用配置
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败！错误码: 0x%x", ret);
        return -1;
    }
    ebtn_init(btns, EBTN_ARRAY_SIZE(btns), NULL, 0, prv_btn_get_state, event_cb);

    // // led gpio
    // io_conf.pin_bit_mask = (1ULL << LED_PIN);
    // io_conf.mode         = GPIO_MODE_OUTPUT;
    // ret                  = gpio_config(&io_conf);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "GPIO配置失败！错误码: 0x%x", ret);
    //     return -1;
    // }
    // gpio_set_level(LED_PIN, 1);  // off
    return 0;
}
