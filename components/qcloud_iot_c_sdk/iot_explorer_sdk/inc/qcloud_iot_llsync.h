/**
 * @file qcloud_iot_llsync.h
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

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_LLSYNC_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_LLSYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "qcloud_iot_llsync_config.h"
#include "qcloud_iot_link.h"
#include "qcloud_iot_data_template_config.h"

// msg header define, bit 7-6 is msg type, bit 5 means request or reply, bit 4 - 0 is id
#define BLE_QIOT_PARSE_MSG_HEAD_TYPE(_C)   (((_C)&0XFF) >> 6)
#define BLE_QIOT_PARSE_MSG_HEAD_EFFECT(_C) ((((_C)&0XFF) & 0X20) ? BLE_QIOT_EFFECT_REPLY : BLE_QIOT_EFFECT_REQUEST)
#define BLE_QIOT_PARSE_MSG_HEAD_ID(_C)     ((_C)&0X1F)

// message type, reference data template
typedef enum {
    BLE_QIOT_MSG_TYPE_PROPERTY = 0,
    BLE_QIOT_MSG_TYPE_EVENT,
    BLE_QIOT_MSG_TYPE_ACTION,
    BLE_QIOT_MSG_TYPE_BUTT,
} BLELLsyncMsgType;

// data type in template, corresponding to type in json file
typedef enum {
    BLE_QIOT_DATA_TYPE_BOOL = 0,
    BLE_QIOT_DATA_TYPE_INT,
    BLE_QIOT_DATA_TYPE_STRING,
    BLE_QIOT_DATA_TYPE_FLOAT,
    BLE_QIOT_DATA_TYPE_ENUM,
    BLE_QIOT_DATA_TYPE_TIME,
    BLE_QIOT_DATA_TYPE_STRUCT,
    BLE_QIOT_DATA_TYPE_ARRAY,
    BLE_QIOT_DATA_TYPE_BUTT,
} BLELLsyncDataType;

typedef enum {
    TYPE_LLSYNC_SUBDEV_RPT_PROPERTY_REPORT,
    TYPE_LLSYNC_SUBDEV_RPT_CONTROL_REPLY,
    TYPE_LLSYNC_SUBDEV_RPT_GET_STATUS,
    TYPE_LLSYNC_SUBDEV_RPT_EVENT_POST,
    TYPE_LLSYNC_SUBDEV_RPT_ACTION_REPLY,
    TYPE_LLSYNC_SUBDEV_RPT_GROUP_REPLY,
    TYPE_LLSYNC_SUBDEV_RPT_SCENE_REPLY,
} BLELLsyncSubdevRptType;
// define tlv struct
typedef struct {
    uint8_t  type;
    uint8_t  id;
    uint16_t len;
    char    *val;
} BLELLsyncTlv;

typedef enum {
    IOT_LLSYNC_SELF_DATA,
    IOT_LLSYNC_OTA_DATA,
    IOT_LLSYNC_SUBDEV_DATA,
    IOT_LLSYNC_SCENE_DATA,
    IOT_LLSYNC_GROUP_DATA,
} LLTlvCtrlType;

typedef enum {
    BLE_QIOT_OTA_SUCCESS     = 0,  // ota success
    BLE_QIOT_OTA_ERR_CRC     = 1,  // ota failed because crc error
    BLE_QIOT_OTA_ERR_TIMEOUT = 2,  // ota failed because download timeout
    BLE_QIOT_OTA_DISCONNECT  = 3,  // ota failed because ble disconnect
    BLE_QIOT_OTA_ERR_FILE    = 4,  // ota failed because the file mismatch the device
} LLsyncOtaErrType;

enum {
    BLE_OTA_ENABLE              = 1,  // ota enable
    BLE_OTA_DISABLE_LOW_POWER   = 2,  // the device low power can not upgrade
    BLE_OTA_DISABLE_LOW_VERSION = 3,  // disable upgrade low version
};

typedef struct {
    LLTlvCtrlType type;
    union {
        char       group_id[64];
        char       scene_id[64];
        DeviceInfo dev_info;
    };
    uint8_t  report_type;
    uint16_t payload_length;
    uint8_t  client_token;
    uint8_t *llsync_data;
    int      llsync_data_len;
} IOTBLELLsyncData;

#define DEFAULT_IOT_BLE_LLSYNC_DATA                                                                \
    {                                                                                              \
        .type = 0, .report_type = 0, .client_token = 0, .llsync_data = NULL, .llsync_data_len = 0, \
    }

typedef struct {
    int (*set_wifi_mode)(TCIoTWifiMode mode, void *usr_data);
    int (*set_wifi_info)(const char *ssid, size_t ssid_len, const char *pwd, size_t pwd_len, void *usr_data);
    int (*connect_wifi)(size_t timeout_ms, void *usr_data);
    int (*set_wifi_token)(const char *token, size_t token_len, void *usr_data);
    int (*get_wifi_log)(void *usr_data);
} IOTBLELLsyncWifiCallback;

#if BLE_QIOT_SUPPORT_OTA

typedef struct {
    uint32_t (*get_download_addr)(void *usr_data);
    int (*read_flash)(void *usr_data, uint32_t read_addr, uint8_t *read_data, uint32_t read_len);
    int (*write_flash)(void *usr_data, uint32_t write_addr, uint8_t *write_data, uint32_t write_len);
    void *(*create_ota_timer)(void *usr_data, void(ota_timer_callback)(void *timer));
    int (*start_ota_timer)(void *usr_data, void *timer, uint32_t timeout_ms);
    int (*stop_ota_timer)(void *usr_data, void *timer);
    int (*delete_ota_timer)(void *usr_data, void *timer);
    int (*allow_ota)(void *usr_data, const char *remote_version);
    void (*ota_start)(void *usr_data);
    void (*ota_stop)(void *usr_data, LLsyncOtaErrType result);
    int (*valid_file_cb)(void *usr_data, uint32_t file_size, const char *file_version);
} IOTBLELLsyncOtaCallback;

#define DEFAULT_BLE_LLSYNC_OTA_CALLBACK                                                                       \
    {                                                                                                         \
        HAL_OTA_get_download_addr, HAL_OTA_read_flash, HAL_OTA_write_flash, HAL_OTA_create_ota_timer,         \
            HAL_OTA_start_ota_timer, HAL_OTA_stop_ota_timer, HAL_OTA_delete_ota_timer, NULL, NULL, NULL, NULL \
    }

#endif

typedef struct {
    void (*connect_callback)(void *usr_data);
    void (*disconnect_callback)(void *usr_data);
    void (*secure_bind_callback)(void *usr_data);
#if BLE_QIOT_SUPPORT_OTA
    IOTBLELLsyncOtaCallback ota_callback;
#endif
} IOTBLELLsyncCallback;

typedef struct {
    IOTBLELLsyncCallback callback;
    DeviceInfo          *dev_info;
    uint32_t             start_adv;  // unit : s
    void                *usr_data;
} IOTBLELLsyncInitParams;

#if BLE_QIOT_SUPPORT_OTA
#define DEFAULT_LLSYNC_CALLBACK                           \
    {                                                     \
        NULL, NULL, NULL, DEFAULT_BLE_LLSYNC_OTA_CALLBACK \
    }
#else
#define DEFAULT_LLSYNC_CALLBACK \
    {                           \
        NULL, NULL, NULL, NULL  \
    }
#endif

#define DEFAULT_LLSYNC_INIT_PARAMS              \
    {                                           \
        DEFAULT_LLSYNC_CALLBACK, NULL, 30, NULL \
    }

/**
 * @brief init llsync
 *
 * @param callback callback to user
 * @param usr_data
 * @return 0 for success
 */
int iot_llsync_init(IOTBLELLsyncInitParams init_params);

/**
 * @brief check iot llsync init
 *
 * @return IotBool
 */
IotBool iot_llsync_is_init(void);

/**
 * @brief llsync data send
 *
 * @param[in] char_uuid characteristic uuid
 * @param[in] data send data buffer
 * @param[in] data_len send data length
 * @return 0 for success
 */
int iot_llsync_send(uint16_t char_uuid, uint8_t *data, size_t data_len);

/**
 * @brief start ble advertising
 *
 * @return 0 for success
 */
int iot_llsync_start_adv(uint32_t adv_time);

/**
 * @brief stop ble advertising
 *
 * @return 0 for success
 */
int iot_llsync_stop_adv(void);

/**
 * @brief creat llsync adv data
 *
 * @param[out] adv_data_raw adv data raw data buffer. length >= 32
 * @return adv data length
 */
int iot_llsync_creat_adv_data(uint8_t adv_data_raw[32]);

/**
 * @brief register wifi callback function
 *
 * @param callback wifi callback function
 * @param usr_data
 */
void iot_llsync_register_wifi_callback(IOTBLELLsyncWifiCallback callback, void *usr_data);

/**
 * @brief unregister wifi callback function
 *
 */
void iot_llsync_unregister_wifi_callback(void);

/**
 * @brief parse lltlv data to data template
 *
 * @param[in,out] data_template data template
 * @param[in] in_buf lltlv data buffer
 * @param[in] buf_len lltlv data buffer length
 * @return 0 for success
 */
int iot_llsync_data_template_parse(DataTemplate *data_template, const char *in_buf, int buf_len);

/**
 * @brief pase data template to lltlv
 *
 * @param[out] out_buf lltlv data buffer
 * @param[in] buf_len lltlv data buffer length
 * @param[in,out] data_template data template
 * @return real pase lltlv data length
 */
int iot_llsync_data_template_property_construct(char *out_buf, int buf_len, DataTemplate *data_template);

/**
 * @brief lltlv to tlv
 *
 * @param[in] lltlv_buf lltlv buffer
 * @param[in] lltlv_len lltlv buffer length
 * @param[out] tlv_buf tlv buffer
 * @param[in] tlv_len  tlv buffer length
 * @return real parse tlv length
 */
int iot_llsync_lltlv_to_tlv(uint8_t *lltlv_buf, uint16_t lltlv_len, uint8_t *tlv_buf, uint16_t tlv_len);

/**
 * @brief llsync uplink data handle
 *
 * @param[in,out] data @see IOTBLELLsyncData
 * @return 0 for success
 */
int iot_llsync_up_data_handle(IOTBLELLsyncData *data);

/**
 * @brief llsync down data handle
 *
 * @param[in,out] data @see IOTBLELLsyncData
 * @return 0 for success
 */
int iot_llsync_down_data_handle(IOTBLELLsyncData *data);

/**
 * @brief llsync get ota download buffer
 *
 * @param[out] buffer if success return ota download buffer others return NULL
 * @param[out] buffer_size if success return ota download buffer size others return 0
 * @return  0 for success
 */
int iot_llsync_get_ota_buffer(uint8_t **buffer, uint32_t *buffer_size);

int iot_llsync_set_core_data(uint8_t bind_state, uint32_t local_psk, uint8_t identify_str[8]);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_IOT_LLSYNC_H_
