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