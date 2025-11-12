/**
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
 * @file qcloud_iot_link_ble_esp32s3.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-12-29
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-12-29 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */
#include "qcloud_iot_link.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_timer.h"
#include "esp_gattc_api.h"

#include "qcloud_iot_llsync_config.h"

#define LLSYNC_LOG_TAG "LLSYNC"

/* Attributes State Machine */
enum {
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_B,
    IDX_CHAR_VAL_B,
    IDX_CHAR_C,
    IDX_CHAR_VAL_C,
    IDX_CHAR_CFG_C,
    IDX_CHAR_D,
    IDX_CHAR_VAL_D,
    IDX_CHAR_E,
    IDX_CHAR_VAL_E,
    IDX_CHAR_F,
    IDX_CHAR_VAL_F,
    IDX_CHAR_CFG_F,
    HRS_IDX_NB
};

typedef struct {
    IotLinkCallback    callback;
    void              *usr_data;
    uint16_t           llsync_handle_table[HRS_IDX_NB];
    esp_timer_handle_t adv_stop_timer_handle;
    uint16_t           mtu;
} LinkBle;

static LinkBle sg_link_ble;

// -------------------------------------------------------------
// service & Characteristic define
// -------------------------------------------------------------

/* The max length of characteristic value. When the GATT client performs a write or prepare write operation,
 *  the data length must be less than LLSYNC_CHAR_VAL_LEN_MAX.
 */
#define LLSYNC_CHAR_VAL_LEN_MAX 500

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_write          = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_write_no_rsp   = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t char_prop_notify         = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t heart_measurement_ccc[2] = {0x00, 0x00};

#define CHARACTERISTIC_DECLARATION(prop)                                                            \
    {                                                                                               \
        {ESP_GATT_AUTO_RSP},                                                                        \
        {                                                                                           \
            ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, 1, 1, prop \
        }                                                                                           \
    }
#define CHARACTERISTIC_VALUE(uuid, perm)                                   \
    {                                                                      \
        {ESP_GATT_AUTO_RSP},                                               \
        {                                                                  \
            ESP_UUID_LEN_128, uuid, perm, LLSYNC_CHAR_VAL_LEN_MAX, 0, NULL \
        }                                                                  \
    }
#define CHARACTERISTIC_CONFIG()                                                                                  \
    {                                                                                                            \
        {ESP_GATT_AUTO_RSP},                                                                                     \
        {                                                                                                        \
            ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, \
                sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc                \
        }                                                                                                        \
    }

/* service_uuid and Characteristic_uuid*/
static uint8_t sg_llsync_uuid[][16] = {
    // service
#if BLE_QIOT_LLSYNC_CONFIG_NET
    [IDX_SVC] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xf0, 0xff, 0x00, 0x00,},
#else 
    [IDX_SVC] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe8, 0xff, 0x00, 0x00},
#endif
    // device info
    [IDX_CHAR_VAL_A] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe1, 0xff, 0x00, 0x00},
    // data
    [IDX_CHAR_VAL_B] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe2, 0xff, 0x00, 0x00},
    // event
    [IDX_CHAR_VAL_C] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe3, 0xff, 0x00, 0x00},
    // ota
    [IDX_CHAR_VAL_D] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe4, 0xff, 0x00, 0x00},
    // sub ctrl
    [IDX_CHAR_VAL_E] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe5, 0xff, 0x00, 0x00},
    // sub rpt
    [IDX_CHAR_VAL_F] = {0xe2, 0xa4, 0x1b, 0x54, 0x93, 0xe4, 0x6a, 0xb5, 0x20, 0x4e, 0xd0, 0x65, 0xe6, 0xff, 0x00, 0x00},
};

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t sg_gatt_db[HRS_IDX_NB] = {
    // Service Declaration
    [IDX_SVC]        = {{ESP_GATT_AUTO_RSP},
                        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
                         sizeof(sg_llsync_uuid[IDX_SVC]), sizeof(sg_llsync_uuid[IDX_SVC]), sg_llsync_uuid[IDX_SVC]}},
    [IDX_CHAR_A]     = CHARACTERISTIC_DECLARATION((uint8_t *)&char_prop_write),
    [IDX_CHAR_VAL_A] = CHARACTERISTIC_VALUE(sg_llsync_uuid[IDX_CHAR_VAL_A], ESP_GATT_PERM_WRITE),
    [IDX_CHAR_B]     = CHARACTERISTIC_DECLARATION((uint8_t *)&char_prop_write),
    [IDX_CHAR_VAL_B] = CHARACTERISTIC_VALUE(sg_llsync_uuid[IDX_CHAR_VAL_B], ESP_GATT_PERM_WRITE),
    [IDX_CHAR_C]     = CHARACTERISTIC_DECLARATION((uint8_t *)&char_prop_notify),
    [IDX_CHAR_VAL_C] = CHARACTERISTIC_VALUE(sg_llsync_uuid[IDX_CHAR_VAL_C], 0),
    [IDX_CHAR_CFG_C] = CHARACTERISTIC_CONFIG(),
    [IDX_CHAR_D]     = CHARACTERISTIC_DECLARATION((uint8_t *)&char_prop_write_no_rsp),
    [IDX_CHAR_VAL_D] = CHARACTERISTIC_VALUE(sg_llsync_uuid[IDX_CHAR_VAL_D], ESP_GATT_PERM_WRITE),
    [IDX_CHAR_E]     = CHARACTERISTIC_DECLARATION((uint8_t *)&char_prop_write),
    [IDX_CHAR_VAL_E] = CHARACTERISTIC_VALUE(sg_llsync_uuid[IDX_CHAR_VAL_E], ESP_GATT_PERM_WRITE),
    [IDX_CHAR_F]     = CHARACTERISTIC_DECLARATION((uint8_t *)&char_prop_notify),
    [IDX_CHAR_VAL_F] = CHARACTERISTIC_VALUE(sg_llsync_uuid[IDX_CHAR_VAL_F], 0),
    [IDX_CHAR_CFG_F] = CHARACTERISTIC_CONFIG(),
};

static void _recv_char_val(uint16_t handle, uint8_t index_array[], size_t index_array_len, void *data, size_t data_len)
{
    for (size_t i = 0; i < index_array_len; i++) {
        if (handle == sg_link_ble.llsync_handle_table[index_array[i]]) {
            sg_link_ble.callback.recv_callback(sg_link_ble.usr_data, &sg_llsync_uuid[index_array[i]][12], 2, data,
                                               data_len);
            break;
        }
    }
}

static uint16_t _get_char_val_handle(uint16_t addr, uint8_t index_array[], size_t index_array_len)
{
    for (size_t i = 0; i < index_array_len; i++) {
        if (addr == *(uint16_t *)&sg_llsync_uuid[index_array[i]][12]) {
            return sg_link_ble.llsync_handle_table[index_array[i]];
        }
    }
    return 0;
}

// -------------------------------------------------------------
// advertising
// -------------------------------------------------------------

static int _advertising_start(uint8_t *raw_adv_data, size_t raw_adv_data_len)
{
    esp_ble_adv_params_t adv_params = {
        .adv_int_min       = 0x20,
        .adv_int_max       = 0x40,
        .adv_type          = ADV_TYPE_IND,
        .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
        .channel_map       = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    // config adv data
    esp_err_t ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, raw_adv_data_len);
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "config adv data failed, error code = %x", ret);
    }

    ESP_LOGI(LLSYNC_LOG_TAG, "start advertising");
    return esp_ble_gap_start_advertising(&adv_params);
}

static int _advertising_stop(void)
{
    return esp_ble_gap_stop_advertising();
}

// -------------------------------------------------------------
// prepare env
// -------------------------------------------------------------
#define PREPARE_BUF_MAX_SIZE 1024

typedef struct {
    uint8_t *prepare_buf;
    int      prepare_len;
    uint16_t handle;
} prepare_type_env_t;

static prepare_type_env_t sg_prepare_write_env;

static void example_prepare_write_event_env(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(LLSYNC_LOG_TAG, "prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
    esp_gatt_status_t status = ESP_GATT_OK;
    if (sg_prepare_write_env.prepare_buf == NULL) {
        sg_prepare_write_env.prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        sg_prepare_write_env.prepare_len = 0;
        sg_prepare_write_env.handle      = 0;
        if (sg_prepare_write_env.prepare_buf == NULL) {
            ESP_LOGE(LLSYNC_LOG_TAG, "%s, Gatt_server prep no mem", __func__);
            status = ESP_GATT_NO_RESOURCES;
        }
    } else {
        if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }
    /*send response when param->write.need_rsp is true */
    if (param->write.need_rsp) {
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp) {
            gatt_rsp->attr_value.len      = param->write.len;
            gatt_rsp->attr_value.handle   = param->write.handle;
            gatt_rsp->attr_value.offset   = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err =
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK) {
                ESP_LOGE(LLSYNC_LOG_TAG, "Send response error");
            }
            free(gatt_rsp);
        } else {
            ESP_LOGE(LLSYNC_LOG_TAG, "%s, malloc failed", __func__);
        }
    }
    if (status != ESP_GATT_OK) {
        return;
    }
    memcpy(sg_prepare_write_env.prepare_buf + param->write.offset, param->write.value, param->write.len);
    sg_prepare_write_env.prepare_len += param->write.len;
    sg_prepare_write_env.handle = param->write.handle;
}

static void example_exec_write_event_env(esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && sg_prepare_write_env.prepare_buf) {
        ESP_LOG_BUFFER_HEX("recv ", sg_prepare_write_env.prepare_buf, sg_prepare_write_env.prepare_len);
        uint8_t index_array[] = {IDX_CHAR_VAL_A, IDX_CHAR_VAL_B};
        _recv_char_val(sg_prepare_write_env.handle, index_array, sizeof(index_array), sg_prepare_write_env.prepare_buf,
                       sg_prepare_write_env.prepare_len);
        // esp_log_buffer_hex(LLSYNC_LOG_TAG, sg_prepare_write_env.prepare_buf, sg_prepare_write_env.prepare_len);
    } else {
        ESP_LOGI(LLSYNC_LOG_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (sg_prepare_write_env.prepare_buf) {
        free(sg_prepare_write_env.prepare_buf);
        sg_prepare_write_env.prepare_buf = NULL;
    }
    sg_prepare_write_env.prepare_len = 0;
}

// -------------------------------------------------------------
// advertising
// -------------------------------------------------------------

#define PROFILE_NUM        1
#define PROFILE_APP_IDX    0
#define LLSYNC_APP_ID      0x55
#define SAMPLE_DEVICE_NAME "l"
#define SVC_INST_ID        0

struct gatts_profile_inst {
    esp_gatts_cb_t       gatts_cb;
    uint16_t             gatts_if;
    uint16_t             app_id;
    uint16_t             conn_id;
    uint16_t             service_handle;
    esp_gatt_srvc_id_t   service_id;
    uint16_t             char_handle;
    esp_bt_uuid_t        char_uuid;
    esp_gatt_perm_t      perm;
    esp_gatt_char_prop_t property;
    uint16_t             descr_handle;
    esp_bt_uuid_t        descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by
 * ESP_GATTS_REG_EVT*/
static struct gatts_profile_inst sg_llsync_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] =
        {
            .gatts_cb = gatts_profile_event_handler,
            .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        },
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            ESP_LOGI(LLSYNC_LOG_TAG, "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT");
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(LLSYNC_LOG_TAG, "advertising start failed");
            } else {
                ESP_LOGI(LLSYNC_LOG_TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(LLSYNC_LOG_TAG, "Advertising stop failed");
            } else {
                ESP_LOGI(LLSYNC_LOG_TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(LLSYNC_LOG_TAG,
                     "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, "
                     "timeout = %d",
                     param->update_conn_params.status, param->update_conn_params.min_int,
                     param->update_conn_params.max_int, param->update_conn_params.conn_int,
                     param->update_conn_params.latency, param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
            if (set_dev_name_ret) {
                ESP_LOGE(LLSYNC_LOG_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(sg_gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret) {
                ESP_LOGE(LLSYNC_LOG_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        } break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(LLSYNC_LOG_TAG, "ESP_GATTS_READ_EVT");
            break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep) {
                // the data length of gattc write  must be less than LLSYNC_CHAR_VAL_LEN_MAX.
                // esp_log_buffer_hex("recv", param->write.value, param->write.len);
                // ESP_LOGI(LLSYNC_LOG_TAG, "param->write.handle : %d", param->write.handle);
                uint8_t index_array[] = {IDX_CHAR_VAL_A, IDX_CHAR_VAL_B, IDX_CHAR_VAL_D, IDX_CHAR_VAL_E};
                _recv_char_val(param->write.handle, index_array, sizeof(index_array), param->write.value,
                               param->write.len);
                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK,
                                                NULL);
                }
            } else {
                example_prepare_write_event_env(gatts_if, param);
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            // the length of gattc prepare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            example_exec_write_event_env(param);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(LLSYNC_LOG_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            param->mtu.mtu = ESP_GATT_DEF_BLE_MTU_SIZE;
            sg_link_ble.callback.sync_mtu_callback(sg_link_ble.usr_data, param->mtu.mtu);
            sg_link_ble.mtu = param->mtu.mtu;
            break;
        case ESP_GATTS_CONF_EVT:
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(LLSYNC_LOG_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status,
                     param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:
            esp_log_buffer_hex(LLSYNC_LOG_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters
             * restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;  // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;  // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;   // timeout = 400*10ms = 4000ms
            // start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            sg_link_ble.callback.connect_callback(sg_link_ble.usr_data, NULL, 0);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(LLSYNC_LOG_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            sg_link_ble.callback.disconnect_callback(sg_link_ble.usr_data, NULL, 0);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGE(LLSYNC_LOG_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            } else if (param->add_attr_tab.num_handle != HRS_IDX_NB) {
                ESP_LOGE(LLSYNC_LOG_TAG,
                         "create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)",
                         param->add_attr_tab.num_handle, HRS_IDX_NB);
            } else {
                ESP_LOGI(LLSYNC_LOG_TAG, "create attribute table successfully, the number handle = %d\n",
                         param->add_attr_tab.num_handle);
                memcpy(sg_link_ble.llsync_handle_table, param->add_attr_tab.handles,
                       sizeof(sg_link_ble.llsync_handle_table));
                esp_ble_gatts_start_service(sg_link_ble.llsync_handle_table[IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            sg_llsync_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(LLSYNC_LOG_TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == sg_llsync_profile_tab[idx].gatts_if) {
                if (sg_llsync_profile_tab[idx].gatts_cb) {
                    sg_llsync_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

static esp_err_t _ble_service_init(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret                               = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "gatts register error, error code = %x", ret);
        return ret;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "gap register error, error code = %x", ret);
        return ret;
    }

    ret = esp_ble_gatts_app_register(LLSYNC_APP_ID);
    if (ret) {
        ESP_LOGE(LLSYNC_LOG_TAG, "gatts app register error, error code = %x", ret);
        return ret;
    }

    return ret;
}

void ble_adv_stop_timer_callback(void *arg)
{
    ESP_LOGI(LLSYNC_LOG_TAG, "stop ble adv");
    _advertising_stop();
}

// -------------------------------------------------------------
// qcloud iot link ble
// -------------------------------------------------------------

void *qcloud_iot_link_ble_init(IotLinkCallback callback, void *usr_data)
{
    static uint8_t is_ble_init = 0;
    if (is_ble_init) {
        return &sg_link_ble;
    }
    memset(&sg_link_ble, 0, sizeof(sg_link_ble));
    sg_link_ble.callback = callback;
    sg_link_ble.usr_data = usr_data;

    int rc = _ble_service_init();
    if (rc) {
        return NULL;
    }
    esp_timer_create_args_t timer_args = {.callback = ble_adv_stop_timer_callback, .name = "ble_adv_stop_timer"};
    esp_err_t               err        = esp_timer_create(&timer_args, &sg_link_ble.adv_stop_timer_handle);
    if (err != ESP_OK) {
        ESP_LOGE(LLSYNC_LOG_TAG, "Failed to create timer (0x%x)", err);
        return NULL;
    }
    is_ble_init = 1;
    return &sg_link_ble;
}

void qcloud_iot_link_ble_deinit(void *handle)
{
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    memset(&sg_link_ble, 0, sizeof(sg_link_ble));
}

int qcloud_iot_link_ble_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                             uint32_t timeout_ms)
{
    Log_d("send to %04x", *(uint16_t *)addr);
    uint16_t character_addr = *(uint16_t *)addr;
    uint8_t  index_array[]  = {IDX_CHAR_VAL_C, IDX_CHAR_VAL_F};
    return esp_ble_gatts_send_indicate(
        sg_llsync_profile_tab[PROFILE_APP_IDX].gatts_if, sg_llsync_profile_tab[PROFILE_APP_IDX].conn_id,
        _get_char_val_handle(character_addr, index_array, sizeof(index_array)), data_len, data, TCIOT_BOOL_FALSE);
}

int qcloud_iot_link_ble_get_uuid(void *handle, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len)
{
    char *address = (char *)esp_bt_dev_get_address();
    memcpy(uuid, address, 6);
    return 0;
}

int qcloud_iot_link_ble_start_advertising(void *handle, void *adv_data, size_t adv_data_len, uint32_t adv_time)
{
    if (adv_time) {
        int64_t timer_interval = adv_time * 1000 * 1000;  // adv_time seconds in microseconds
        esp_timer_stop(sg_link_ble.adv_stop_timer_handle);
        esp_err_t err = esp_timer_start_once(sg_link_ble.adv_stop_timer_handle, timer_interval);
        if (err != ESP_OK) {
            ESP_LOGE(LLSYNC_LOG_TAG, "Failed to start timer (0x%x)", err);
            return QCLOUD_ERR_FAILURE;
        }
    }
    return _advertising_start((uint8_t *)adv_data, adv_data_len);
}

int qcloud_iot_link_ble_stop_advertising(void *handle)
{
    return _advertising_stop();
}

int qcloud_iot_link_ble_get_mtu(void *handle)
{
    return sg_link_ble.mtu;
}