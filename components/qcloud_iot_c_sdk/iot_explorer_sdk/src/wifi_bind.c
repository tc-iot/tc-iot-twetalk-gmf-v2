/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2023 THL A29 Limited, a Tencent company.All rights reserved.
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
 * @file wifi_bind.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2023-01-03
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2023-01-03 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "qcloud_iot_wifi_bind.h"
#include "service_mqtt.h"

typedef struct {
    IotBool token_received;
    int bind_result;
} WifiBindContext;

static const char* sg_service_method_str[] = {
    [0] = "app_bind_token_reply",
};

// -------------------------------------------------------------------------------------------------
// wifi bind
// -------------------------------------------------------------------------------------------------

typedef struct {
    uint64_t mqtt_start_time;
    uint64_t mqtt_connected_time;
    uint64_t token_publish_time;
} WifiBindMqttTime;

static void _service_callback(void* client, const MQTTMessage* message, void* usr_data)
{
    // {"method":"app_bind_token_reply","clientToken":"tech0-91582","code":0,"status":"success"}
    WifiBindContext* ctx = (WifiBindContext*)usr_data;
    ctx->token_received  = TCIOT_BOOL_TRUE;
    utils_json_get_int("code", strlen("code"), message->payload_str, message->payload_len, &ctx->bind_result);
}

static int _wifi_bind_service_init(void* client)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    WifiBindContext* ctx = (WifiBindContext*)TCI_HAL_Malloc(sizeof(WifiBindContext));
    if (!ctx) {
        return QCLOUD_ERR_MALLOC;
    }
    ctx->token_received = TCIOT_BOOL_FALSE;
    ctx->bind_result    = -1;

    // 1. check service topic sub
    int rc = service_mqtt_init(client);
    if (rc) {
        return rc;
    }

    ServiceRegisterParams params = {
        .type           = SERVICE_TYPE_WIFI_BIND,
        .method_list    = sg_service_method_str,
        .method_num     = sizeof(sg_service_method_str) / sizeof(sg_service_method_str[0]),
        .message_handle = _service_callback,
        .usr_data       = ctx,
        .user_data_free = TCI_HAL_Free,
    };
    // 2. register misc service service
    rc = service_mqtt_service_register(client, &params);
    if (rc) {
        TCI_HAL_Free(ctx);
    }
    return rc;
}

static void _wifi_bind_service_deinit(void* client)
{
    POINTER_SANITY_CHECK_RTN(client);
    service_mqtt_service_unregister(client, SERVICE_TYPE_WIFI_BIND);
    service_mqtt_deinit(client);
}

static int _wifi_bind_report(void* client, IotWifiBindType type, const char* token, const IotWifiBindTime* bind_time,
                             WifiBindMqttTime* mqtt_time)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);

    const char* type_str[] = {
        [TCIOT_WIFI_BIND_TYPE_SOFT_AP]       = "SoftAP",
        [TCIOT_WIFI_BIND_TYPE_SMART_CONFIG]  = "SmartConfig",
        [TCIOT_WIFI_BIND_TYPE_AIR_KISS]      = "AirKiss",
        [TCIOT_WIFI_BIND_TYPE_LLSYNC_BLE]    = "BLE LLSync",
        [TCIOT_WIFI_BIND_TYPE_SIMPLE_CONFIG] = "SimpleConfig",
        [TCIOT_WIFI_BIND_TYPE_CUSTOM_BLE]    = "BLE Custom",
    };

    char payload[512] = {0};

    int len =
        TCI_HAL_Snprintf(payload, sizeof(payload),
                         "{\"method\":\"app_bind_token\",\"clientToken\":\"app-bind-%d\",\"params\":{\"token\":\"%s\","
                         "\"pairTime\":{\"type\":\"%s\",\"start\":%llu,\"getSSID\":%llu,\"wifiConnected\":"
                         "%llu,\"getToken\":%llu,\"mqttStart\":%llu,\"mqttConnected\":%llu,\"tokenPublish\":%llu}}}",
                         TCI_HAL_Random(), token, type_str[type], bind_time->start_time, bind_time->get_ssid_time,
                         bind_time->wifi_connected_time, bind_time->get_token_time, mqtt_time->mqtt_start_time,
                         mqtt_time->mqtt_connected_time, mqtt_time->token_publish_time);
    return service_mqtt_publish(client, QOS0, payload, len);
}

static int _wifi_bind_wait_for_reply(void* client, uint64_t timeout_ms)
{
    WifiBindContext* ctx = service_mqtt_service_get_usr_data(client, SERVICE_TYPE_WIFI_BIND);
    if (!ctx) {
        return QCLOUD_ERR_FAILURE;
    }

    TCI_Timer timer;
    TCI_HAL_TimerCountdownMs(&timer, timeout_ms);
    Log_d("wait app_bind_token_reply....");
    while (!ctx->token_received && !TCI_HAL_TimerExpired(&timer)) {
        TCIOT_MQTT_Yield(client, 200);
    }

    if (!ctx->token_received) {
        return QCLOUD_ERR_WIFI_CONFIG_TIMEOUT;
    }

    return !ctx->bind_result ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

// -------------------------------------------------------------------------------------------------
// API
// -------------------------------------------------------------------------------------------------

/**
 * @brief Send wifi bind msg and wait for reply.
 *
 * @param[in] type @see IotWifiBindType
 * @param[in] token token get from mini program
 * @param[in] bind_time using for debug @see IotWifiBindTime
 * @param[in] timeout_ms max timeout wait for reply
 * @param[in] event_callback
 * @return @see IotReturnCode
 */
int TCIOT_WifiBind_Sync(IotWifiBindType type, const char* token, const IotWifiBindTime* bind_time, uint64_t timeout_ms,
                        void (*event_callback)(IotWifiBindEvent event))
{
    POINTER_SANITY_CHECK(token, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(event_callback, QCLOUD_ERR_INVAL);

    MQTTInitParams init_params = DEFAULT_MQTT_INIT_PARAMS;

    WifiBindMqttTime mqtt_time = {0};

    DeviceInfo device_info;
    init_params.device_info = &device_info;

    int rc = TCI_HAL_GetDevInfo((uint8_t*)&device_info, sizeof(DeviceInfo));
    if (rc) {
        return rc;
    }
    mqtt_time.mqtt_start_time = TCI_HAL_GetTimeMs();
    event_callback(TCIOT_WIFI_BIND_EVENT_MQTT_CONNECT_BEGIN);

    void* mqtt_client = TCIOT_MQTT_Construct(&init_params);
    if (!mqtt_client) {
        event_callback(TCIOT_WIFI_BIND_EVENT_MQTT_CONNECT_FAIL);
        return QCLOUD_ERR_MALLOC;
    }
    mqtt_time.mqtt_connected_time = TCI_HAL_GetTimeMs();

    rc = _wifi_bind_service_init(mqtt_client);
    if (rc) {
        goto exit;
    }

    mqtt_time.token_publish_time = TCI_HAL_GetTimeMs();
    event_callback(TCIOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_BEGIN);
    rc = _wifi_bind_report(mqtt_client, type, token, bind_time, &mqtt_time);
    if (rc < 0) {
        goto exit;
    }

    rc = _wifi_bind_wait_for_reply(mqtt_client, timeout_ms);
    rc ? event_callback(TCIOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_FAIL)
       : event_callback(TCIOT_WIFI_BIND_EVENT_MQTT_REPORT_TOKEN_SUCCESS);
exit:
    _wifi_bind_service_deinit(mqtt_client);
    TCIOT_MQTT_Destroy(&mqtt_client);
    return rc;
}
