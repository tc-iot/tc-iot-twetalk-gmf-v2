/**
 * @file HAL_ota_esp.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2023-06-19
 *
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2022 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @par Change Log:
 * <table>
 * Date				Version		Author			Description
 * 2023-06-19		1.0			hubertxxu		first commit
 * </table>
 */

#include "esp_flash.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "qcloud_iot_common.h"

// 获取下一个OTA分区，用于写入新的固件
static const esp_partition_t *sg_next_ota_partition = NULL;
// 用于跟踪已擦除的扇区，避免重复擦除
static uint32_t s_erased_sector_addr = 0xFFFFFFFF;

uint32_t HAL_OTA_get_download_addr(void *usr_data)
{
    // 获取当前运行的分区
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        // 当前运行在OTA0分区，因此将新固件写入OTA1分区
        sg_next_ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    } else if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
        // 当前运行在OTA1分区，因此将新固件写入OTA0分区
        sg_next_ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    }
    if (sg_next_ota_partition == NULL) {
        Log_e("can not find ota partition.");
        return 0;
    }
    Log_i("next ota partition[%s] address : 0x%08x ", sg_next_ota_partition->label, sg_next_ota_partition->address);

    // 重置擦除扇区地址，确保新OTA开始时正确擦除
    s_erased_sector_addr = 0xFFFFFFFF;

    return sg_next_ota_partition->address;
}

int HAL_OTA_read_flash(void *usr_data, uint32_t read_addr, uint8_t *read_data, uint32_t read_len)
{
    if (!sg_next_ota_partition) {
        if (!HAL_OTA_get_download_addr(NULL)) {
            return -1;
        }
    }
    int rc = esp_flash_read(sg_next_ota_partition->flash_chip, read_data, sg_next_ota_partition->address + read_addr,
                            read_len);
    return rc;
}

int HAL_OTA_write_flash(void *usr_data, uint32_t write_addr, uint8_t *write_data, uint32_t write_len)
{
    if (!sg_next_ota_partition) {
        if (!HAL_OTA_get_download_addr(NULL)) {
            return -1;
        }
    }

    // 计算扇区地址（4096字节对齐）
    uint32_t sector_addr = (sg_next_ota_partition->address + write_addr) & ~(4096 - 1);
    int rc               = ESP_OK;

    // 如果当前扇区尚未擦除，则先擦除
    if (s_erased_sector_addr != sector_addr) {
        rc = esp_flash_erase_region(sg_next_ota_partition->flash_chip, sector_addr, 4096);
        if (rc != ESP_OK) {
            Log_e("erase flash fail, addr:0x%08x, rc:%d", sector_addr, rc);
            return rc;
        }
        s_erased_sector_addr = sector_addr;
    }

    // 写入数据
    rc = esp_flash_write(sg_next_ota_partition->flash_chip, write_data, sg_next_ota_partition->address + write_addr,
                         write_len);
    if (rc != ESP_OK) {
        Log_e("write flash fail, addr:0x%08x, len:%d, rc:%d", sg_next_ota_partition->address + write_addr, write_len,
              rc);
        return rc;
    }

    return 0;
}

int HAL_OTA_SwitchToNewFirmware(void)
{
    esp_err_t err = esp_ota_set_boot_partition(sg_next_ota_partition);
    if (err != ESP_OK) {
        Log_e("set boot partition fail: %d", err);
        return err;
    }
    // 重启设备
    Log_w("reboot system now...");
    esp_restart();
    return 0;
}

// OTA定时器结构体，用于存储定时器句柄和回调函数
typedef struct {
    TimerHandle_t timer_handle;
    void (*callback)(void *);
} OTATimerContext;

// FreeRTOS定时器回调函数包装器
static void ota_timer_callback_wrapper(TimerHandle_t xTimer)
{
    OTATimerContext *ctx = (OTATimerContext *)pvTimerGetTimerID(xTimer);
    if (ctx && ctx->callback) {
        ctx->callback((void *)xTimer);
    }
}

void *HAL_OTA_create_ota_timer(void *usr_data, void(ota_timer_callback)(void *timer))
{
    if (ota_timer_callback == NULL) {
        Log_e("ota_timer_callback is NULL");
        return NULL;
    }

    // 分配定时器上下文
    OTATimerContext *ctx = (OTATimerContext *)malloc(sizeof(OTATimerContext));
    if (ctx == NULL) {
        Log_e("malloc OTATimerContext failed");
        return NULL;
    }

    ctx->callback = ota_timer_callback;

    // 创建FreeRTOS软件定时器（单次触发）
    ctx->timer_handle = xTimerCreate("OTA_Timer",           // 定时器名称
                                      pdMS_TO_TICKS(1000),  // 初始周期（1秒，后续会修改）
                                      pdFALSE,              // 单次触发
                                      (void *)ctx,          // 定时器ID（传递上下文）
                                      ota_timer_callback_wrapper);  // 回调函数

    if (ctx->timer_handle == NULL) {
        Log_e("xTimerCreate failed");
        free(ctx);
        return NULL;
    }

    Log_d("OTA timer created: %p", ctx);
    return (void *)ctx;
}

int HAL_OTA_start_ota_timer(void *usr_data, void *timer, uint32_t timeout_ms)
{
    if (timer == NULL) {
        Log_e("timer is NULL");
        return -1;
    }

    OTATimerContext *ctx = (OTATimerContext *)timer;

    // 修改定时器周期
    if (xTimerChangePeriod(ctx->timer_handle, pdMS_TO_TICKS(timeout_ms), 0) != pdPASS) {
        Log_e("xTimerChangePeriod failed");
        return -1;
    }

    // 启动定时器
    if (xTimerStart(ctx->timer_handle, 0) != pdPASS) {
        Log_e("xTimerStart failed");
        return -1;
    }

    Log_d("OTA timer started: %p, timeout: %u ms", timer, timeout_ms);
    return 0;
}

int HAL_OTA_stop_ota_timer(void *usr_data, void *timer)
{
    if (timer == NULL) {
        Log_e("timer is NULL");
        return -1;
    }

    OTATimerContext *ctx = (OTATimerContext *)timer;

    // 停止定时器
    if (xTimerStop(ctx->timer_handle, 0) != pdPASS) {
        Log_e("xTimerStop failed");
        return -1;
    }

    Log_d("OTA timer stopped: %p", timer);
    return 0;
}

int HAL_OTA_delete_ota_timer(void *usr_data, void *timer)
{
    if (timer == NULL) {
        Log_e("timer is NULL");
        return -1;
    }

    OTATimerContext *ctx = (OTATimerContext *)timer;

    // 删除定时器
    if (xTimerDelete(ctx->timer_handle, 0) != pdPASS) {
        Log_e("xTimerDelete failed");
        return -1;
    }

    // 释放上下文内存
    free(ctx);

    Log_d("OTA timer deleted: %p", timer);
    return 0;
}