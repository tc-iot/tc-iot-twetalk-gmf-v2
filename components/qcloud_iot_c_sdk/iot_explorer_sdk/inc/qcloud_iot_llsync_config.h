/**
 * @file qcloud_iot_llsync_config.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-11-25
 *
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2021 THL A29 Limited, a Tencent company.All rights reserved.
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
 * 2022-11-25		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_LLSYNC_CONFIG_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_LLSYNC_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "qcloud_iot_config.h"

// ------------------------------------------------------------------------------
// config
// ------------------------------------------------------------------------------

// choose the ability you used, set 1 with BLE_QIOT_LLSYNC_STANDARD and BLE_QIOT_LLSYNC_CONFIG_NET will enable dual-mode
// communication
#define BLE_QIOT_LLSYNC_STANDARD   0  // support llsync standard
#define BLE_QIOT_LLSYNC_CONFIG_NET 1  // support llsync configure network
#define BLE_QIOT_LLSYNC_GATEWAY    0  // support llsync gateway

#if (1 == BLE_QIOT_LLSYNC_STANDARD) && (1 == BLE_QIOT_LLSYNC_CONFIG_NET)
#define BLE_QIOT_LLSYNC_DUAL_COM 1  // support llsync dual communication, do not use ble ota under the mode
#endif

#define BLE_QIOT_SDK_VERSION "1.5.1"  // sdk version
#define BLE_QIOT_SDK_DEBUG   0        // sdk debug

#define TENCENT_COMPANY_IDENTIFIER 0xFEE7  // Tencent Company ID, another is 0xFEBA

#define ATT_USER_MTU (247)

// the device broadcast is controlled by the user, but we provide a mechanism to help the device save more power.
// if you want broadcast is triggered by something like press a button instead of all the time, and the broadcast
// stopped automatically in a few minutes if the device is not bind, define BLE_QIOT_BUTTON_BROADCAST is 1 and
// BLE_QIOT_BIND_TIMEOUT is the period that broadcast stopped.
// if the device in the bound state, broadcast dose not stop automatically.
#define BLE_QIOT_BUTTON_BROADCAST 0
#if BLE_QIOT_BUTTON_BROADCAST
#define BLE_QIOT_BIND_TIMEOUT (2 * 60 * 1000)  // unit: ms
#endif                                         // BLE_QIOT_BUTTON_BROADCAST

// in some BLE stack the default ATT_MTU is 23, set BLE_QIOT_REMOTE_SET_MTU is 1 if you want to reset the mtu by the
// Tencent Lianlian. Tencent Lianlian will set the mtu get from function ble_get_user_data_mtu_size()
#define BLE_QIOT_REMOTE_SET_MTU (1)

// the following definition will affect the stack that LLSync used，the minimum value tested is 2048 bytes
// the max length of llsync event data, depends on the length of user data reported to Tencent Lianlian at a time
#define BLE_QIOT_EVENT_MAX_SIZE (128)
// the minimum between BLE_QIOT_EVENT_MAX_SIZE and mtu
#define BLE_QIOT_EVENT_BUF_SIZE (23)

// some data like integer need to be transmitted in a certain byte order, defined it according to your device
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __ORDER_BIG_ENDIAN__    4321
#define __BYTE_ORDER__          __ORDER_LITTLE_ENDIAN__

#if BLE_QIOT_LLSYNC_STANDARD
// use property
#define BLE_QIOT_INCLUDE_PROPERTY

#define BLE_QIOT_DYNREG_ENABLE 0
// some users hope to confirm on the device before the binding, set BLE_QIOT_SECURE_BIND is 1 to enable the secure
// binding and enable secure bind in iot-explorer console. When the server is bound, the device callback
// ble_secure_bind_user_cb() will be triggered, the user agree or refuse connect by ble_secure_bind_user_confirm(). If
// the device does not respond and the connection timeout, or the user cancel the connection in Tencent Lianlian, a
// notify will received in function ble_secure_bind_user_notify().
#define BLE_QIOT_SECURE_BIND 0
#if BLE_QIOT_SECURE_BIND
#define BLE_QIOT_BIND_WAIT_TIME 60
#endif  // BLE_QIOT_SECURE_BIND

// define user develop version, pick from "a-zA-Z0-9.-_" and length limits 1～32 bytes.
// must be consistent with the firmware version that user write in the iot-explorer console
// refer https://cloud.tencent.com/document/product/1081/40296
#define BLE_QIOT_USER_DEVELOPER_VERSION "0.0.1"

#define BLE_LLSYNC_CORE_DATA_FILEPATH "/data/ble_llsync_core_data.txt"

#define BLE_QIOT_SUPPORT_OTA 0  // 1 is support ota, others not

#define TX_PPSP_IMPL_CFGS_PROG_ADDR_BASE (0x11000000)
#define TX_PPSP_IMPL_CFGS_PROG_SCTR_SIZE (0x1000)   // program sector size in byte
#define TX_PPSP_IMPL_CFGS_PROG_ADDR_BGNS (0x50000)  // program data bgn address of flash
#define TX_PPSP_IMPL_CFGS_PROG_ADDR_ENDS (0x80000)  // program data end address of flash

#undef BLE_QIOT_RECORD_FLASH_ADDR
// some sdk info needs to stored on the device and the address is up to you
#define BLE_QIOT_RECORD_FLASH_ADDR \
    (TX_PPSP_IMPL_CFGS_PROG_ADDR_BASE + TX_PPSP_IMPL_CFGS_PROG_ADDR_BGNS - TX_PPSP_IMPL_CFGS_PROG_SCTR_SIZE)  // 0xFE000

#define BLE_QIOT_RECORD_FLASH_PAGESIZE 4096  // flash page size, see chip datasheet

#if BLE_QIOT_SUPPORT_OTA
#define BLE_QIOT_SUPPORT_RESUMING 0  // 1 is support resuming, others not
#if BLE_QIOT_SUPPORT_RESUMING
// storage ota info in the flash if support resuming ota file
#define BLE_QIOT_OTA_INFO_FLASH_ADDR (BLE_QIOT_RECORD_FLASH_ADDR + 0x1000)
#endif  // BLE_QIOT_SUPPORT_RESUMING

#define BLE_QIOT_TOTAL_PACKAGES 32  // the total package numbers in a loop
#define BLE_QIOT_PACKAGE_LENGTH 128  // the user data length in package, ble_get_user_data_mtu_size() - 3 is the max
#define BLE_QIOT_RETRY_TIMEOUT  0x5   // the max interval between two packages, unit: second
// the time spent for device reboot, the server waiting the device version reported after upgrade. unit: second
#define BLE_QIOT_REBOOT_TIME      20
#define BLE_QIOT_PACKAGE_INTERVAL 20  // the interval between two packages send by the server
// the package from the server will storage in the buffer, write the buffer to the flash at one time when the buffer
// overflow. reduce the flash write can speed up file download, we suggest the BLE_QIOT_OTA_BUF_SIZE is multiples
// of BLE_QIOT_PACKAGE_LENGTH and equal flash page size
#define BLE_QIOT_OTA_BUF_SIZE (BLE_QIOT_RECORD_FLASH_PAGESIZE)
#endif  // BLE_QIOT_SUPPORT_OTA
#endif  // BLE_QIOT_LLSYNC_STANDARD

#if BLE_QIOT_LLSYNC_DUAL_COM
#define IOT_BLE_UUID_SERVICE 0xFFE8
#else
#if BLE_QIOT_LLSYNC_STANDARD
#define IOT_BLE_UUID_SERVICE 0xFFE0
#endif  // BLE_QIOT_LLSYNC_STANDARD
#if BLE_QIOT_LLSYNC_CONFIG_NET
#define IOT_BLE_UUID_SERVICE 0xFFF0
#endif  // BLE_QIOT_LLSYNC_CONFIG_NET
#endif

#define SWAP_32(x) \
    ((((x)&0xFF000000) >> 24) | (((x)&0x00FF0000) >> 8) | (((x)&0x0000FF00) << 8) | (((x)&0x000000FF) << 24))
#define SWAP_16(x) ((((x)&0xFF00) >> 8) | (((x)&0x00FF) << 8))
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HTONL(x) SWAP_32(x)
#define HTONS(x) SWAP_16(x)
#define NTOHL(x) SWAP_32(x)
#define NTOHS(x) SWAP_16(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define HTONL(x) (x)
#define HTONS(x) (x)
#define NTOHL(x) (x)
#define NTOHS(x) (x)
#else
#error "undefined byte order"
#endif

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_LLSYNC_CONFIG_H_
