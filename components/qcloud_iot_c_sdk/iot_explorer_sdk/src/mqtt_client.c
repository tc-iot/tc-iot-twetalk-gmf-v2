/**
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
 * @file mqtt_client.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-05-28
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-28 <td>1.0     <td>fancyxu   <td>first commit
 * <tr><td>2021-07-07 <td>1.1     <td>fancyxu   <td>support user host for unittest
 * <tr><td>2021-07-08 <td>1.1     <td>fancyxu   <td>fix code standard of IotReturnCode and QcloudIotClient
 * </table>
 */

#include "mqtt_client.h"

#define MQTT_VERSION_3_1_1 4
// 20 for timestamp length & delimiter
#define MAX_MQTT_CONNECT_USR_NAME_LEN (MAX_SIZE_OF_CLIENT_ID + QCLOUD_TCIOT_DEVICE_SDK_APPID_LEN + MAX_CONN_ID_LEN + 20)
#define MAX_MQTT_CONNECT_PASSWORD_LEN (51)

/**************************************************************************************
 * static method
 **************************************************************************************/

/**
 * @brief Get random packet id, when start.
 *
 * @return packet id
 */
static uint16_t _get_random_start_packet_id(void)
{
    return TCI_HAL_Random() % 65536 + 1;
}

/**
 * @brief MQTT yield thread function.
 *
 * @param[in] ptr pointer to mqtt client
 */
static void _mqtt_yield_thread(void* ptr)
{
    int              rc     = QCLOUD_RET_SUCCESS;
    QcloudIotClient* client = (QcloudIotClient*)ptr;

    Log_d("start mqtt_yield_thread...");
    client->yield_thread_exit = 0;
    while (client->yield_thread_running) {
        rc = qcloud_iot_mqtt_yield(client, 200);

        if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
            TCI_HAL_SleepMs(500);
            continue;
        }
        if (rc == QCLOUD_RET_MQTT_MANUALLY_DISCONNECTED || rc == QCLOUD_ERR_MQTT_RECONNECT_TIMEOUT) {
            Log_e("MQTT Yield thread exit with error: %d", rc);
            break;
        }
        if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
            Log_e("MQTT Yield thread error: %d", rc);
        }
    }

    client->yield_thread_running   = 0;
    client->yield_thread_exit_code = rc;
    client->yield_thread_exit      = 1;
}

/**
 * @brief Init list_pub_wait_ack and list_sub_wait_ack
 *
 * @param[in,out] client pointer to mqtt client
 * @return @see IotReturnCode
 */
static int _mqtt_client_list_init(QcloudIotClient* client)
{
    TCIOT_FUNC_ENTRY;

    UtilsListFunc func = DEFAULT_LIST_FUNCS;

    client->list_pub_wait_ack = utils_list_create(func, MAX_REPUB_NUM);
    if (!client->list_pub_wait_ack) {
        Log_e("create pub wait list failed.");
        goto error;
    }

    client->list_sub_wait_ack = utils_list_create(func, QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS);
    if (!client->list_sub_wait_ack) {
        Log_e("create sub wait list failed.");
        goto error;
    }
    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
error:
    utils_list_destroy(client->list_pub_wait_ack);
    client->list_pub_wait_ack = NULL;
    utils_list_destroy(client->list_sub_wait_ack);
    client->list_sub_wait_ack = NULL;
    TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_FAILURE);
}

/**
 * @brief Init network, tcp(with AUTH_WITH_NO_TLS) or tls.
 *
 * @param[in,out] client pointer to mqtt client
 */
static void _mqtt_client_network_init(QcloudIotClient* client, const MQTTInitParams* params)
{
    if (params->host) {
        TCI_HAL_Snprintf(client->host_addr, HOST_STR_LENGTH, "%s", params->host);
    } else {
        // default host
        TCI_HAL_Snprintf(client->host_addr, HOST_STR_LENGTH, "%s.%s", client->device_info->product_id,
                         QCLOUD_TCIOT_MQTT_DIRECT_DOMAIN);
    }

    client->main_host        = params->host;
    client->backup_host      = params->backup_host;
    client->get_next_host_ip = params->get_next_host_ip;
#ifndef ENABLE_AUTH_NO_TLS
    // device param for TLS connection
#ifdef ENABLE_AUTH_MODE_CERT
    Log_d("cert file: %s", STRING_PTR_PRINT_SANITY_CHECK(client->device_info->cert_file));
    Log_d("key file: %s", STRING_PTR_PRINT_SANITY_CHECK(client->device_info->key_file));

    client->network_stack.ssl_connect_params.cert_file  = client->device_info->cert_file;
    client->network_stack.ssl_connect_params.key_file   = client->device_info->key_file;
    client->network_stack.ssl_connect_params.ca_crt     = iot_ca_get();
    client->network_stack.ssl_connect_params.ca_crt_len = strlen(client->network_stack.ssl_connect_params.ca_crt);
#else
    client->network_stack.ssl_connect_params.psk        = (char*)client->device_secret_decode;
    client->network_stack.ssl_connect_params.psk_length = client->device_secret_decode_len;
    memcpy(client->network_stack.ssl_connect_params.psk_id, client->client_id, MAX_SIZE_OF_CLIENT_ID);
    client->network_stack.ssl_connect_params.ca_crt     = NULL;
    client->network_stack.ssl_connect_params.ca_crt_len = 0;
#endif
    client->network_stack.host = client->host_addr;
    client->network_stack.port = MQTT_SERVER_PORT_TLS;
    client->network_stack.ssl_connect_params.timeout_ms =
        client->command_timeout_ms > QCLOUD_TCIOT_TLS_HANDSHAKE_TIMEOUT ? client->command_timeout_ms
                                                                        : QCLOUD_TCIOT_TLS_HANDSHAKE_TIMEOUT;
    client->network_stack.type = TCIOT_NETWORK_TYPE_TLS;
#else
    client->network_stack.host = client->host_addr;
    client->network_stack.port = MQTT_SERVER_PORT_NO_TLS;
    client->network_stack.type = TCIOT_NETWORK_TYPE_TCP;
#endif
    qcloud_iot_network_init(&(client->network_stack));
}

/**
 * @brief Init mqtt connection options.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] params mqtt init params, @see MQTTInitParams
 * @return int
 */
static int _mqtt_client_connect_option_init(QcloudIotClient* client, const MQTTInitParams* params)
{
    TCIOT_FUNC_ENTRY;

    int  rc          = 0;
    long cur_timesec = 0;

    client->options.mqtt_version = MQTT_VERSION_3_1_1;
    // Upper limit of keep alive interval is (11.5 * 60) seconds
    client->options.client_id           = client->client_id;
    client->options.keep_alive_interval = params->keep_alive_interval > 690 ? 690 : params->keep_alive_interval;
    client->options.clean_session       = params->clean_session;

    // calculate user name & password
    client->options.username = (char*)TCI_HAL_Malloc(MAX_MQTT_CONNECT_USR_NAME_LEN);
    if (!client->options.username) {
        Log_e("malloc username failed!");
        rc = QCLOUD_ERR_MALLOC;
        goto error;
    }

    cur_timesec =
        (MAX_ACCESS_EXPIRE_TIMEOUT <= 0) ? 0x7fffffffL : (TCI_HAL_GetTimeSecond() + MAX_ACCESS_EXPIRE_TIMEOUT / 1000);
    get_next_conn_id(client->conn_id);
    TCI_HAL_Snprintf(client->options.username, MAX_MQTT_CONNECT_USR_NAME_LEN, "%s;%s;%s;%ld", client->options.client_id,
                     QCLOUD_TCIOT_DEVICE_SDK_APPID, client->conn_id, cur_timesec);

#if defined(ENABLE_AUTH_NO_TLS) && defined(ENABLE_AUTH_MODE_KEY)
    char sign[41]            = {0};
    client->options.password = (char*)TCI_HAL_Malloc(MAX_MQTT_CONNECT_PASSWORD_LEN);
    if (!client->options.password) {
        Log_e("malloc password failed!");
        rc = QCLOUD_ERR_MALLOC;
        goto error;
    }
    utils_hmac_sha1_hex((const uint8_t*)client->options.username, strlen(client->options.username),
                        (uint8_t*)client->device_secret_decode, client->device_secret_decode_len, sign);
    TCI_HAL_Snprintf(client->options.password, MAX_MQTT_CONNECT_PASSWORD_LEN, "%s;hmacsha1", sign);
#endif
    TCIOT_FUNC_EXIT_RC(rc);
error:
    TCI_HAL_Free(client->options.username);
    client->options.username = NULL;
    TCI_HAL_Free(client->options.password);
    client->options.password = NULL;
    TCIOT_FUNC_EXIT_RC(rc);
}

/**
 * @brief Init mqtt client.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] params mqtt init params, @see MQTTInitParams
 * @return @see IotReturnCode
 *
 * @note
 * 1. init device info.
 * 2. init mutex and var
 * 3. init list @see _mqtt_client_list_init
 * 4. init connect option @see _mqtt_client_connect_option_init
 * 5. init network @see _mqtt_client_network_init
 */
static int _qcloud_iot_mqtt_client_init(QcloudIotClient* client, const MQTTInitParams* params)
{
    TCIOT_FUNC_ENTRY;
    int rc = 0;
    memset(client, 0x0, sizeof(QcloudIotClient));

    // set device info
    client->device_info = params->device_info;
    rc = TCI_HAL_Snprintf(client->client_id, MAX_SIZE_OF_CLIENT_ID, "%s%s", client->device_info->product_id,
                          client->device_info->device_name);
#ifdef ENABLE_AUTH_MODE_KEY
    utils_base64decode(client->device_secret_decode, MAX_SIZE_OF_DECODE_PSK_LENGTH, &client->device_secret_decode_len,
                       client->device_info->device_secret, strlen(client->device_info->device_secret));
#endif
    Log_i("SDK_Ver: %s, Product_ID: %s, Device_Name: %s", QCLOUD_TCIOT_DEVICE_SDK_VERSION,
          client->device_info->product_id, client->device_info->device_name);

    // command timeout should be in range [MIN_COMMAND_TIMEOUT, MAX_COMMAND_TIMEOUT]
    client->command_timeout_ms =
        (params->command_timeout < MIN_COMMAND_TIMEOUT)
            ? MIN_COMMAND_TIMEOUT
            : (params->command_timeout > MAX_COMMAND_TIMEOUT ? MAX_COMMAND_TIMEOUT : params->command_timeout);

    // packet id, random from [1 - 65536]
    client->next_packet_id      = _get_random_start_packet_id();
    client->write_buf_size      = QCLOUD_TCIOT_MQTT_TX_BUF_LEN;
    client->read_buf_size       = QCLOUD_TCIOT_MQTT_RX_BUF_LEN;
    client->event_handle        = params->event_handle;
    client->auto_connect_enable = params->auto_connect_enable;
    client->default_subscribe   = params->default_subscribe;

    client->lock_generic = TCI_HAL_MutexCreate();
    if (!client->lock_generic) {
        goto error;
    }

    client->lock_write_buf = TCI_HAL_MutexCreate();
    if (!client->lock_write_buf) {
        goto error;
    }

    client->lock_yield = TCI_HAL_MutexCreate();
    if (!client->lock_yield) {
        goto error;
    }

    rc = _mqtt_client_list_init(client);

    if (rc) {
        goto error;
    }

    rc = _mqtt_client_connect_option_init(client, params);
    if (rc) {
        goto error;
    }

    _mqtt_client_network_init(client, params);
    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);

error:
    TCI_HAL_MutexDestroy(client->lock_generic);
    client->lock_generic = NULL;
    TCI_HAL_MutexDestroy(client->lock_write_buf);
    client->lock_write_buf = NULL;
    TCI_HAL_MutexDestroy(client->lock_yield);
    client->lock_yield = NULL;
    TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_FAILURE);
}

/**
 * @brief Deinit mqtt client.
 *
 * @param[in,out] client pointer to mqtt client
 */
static void _qcloud_iot_mqtt_client_deinit(QcloudIotClient* client)
{
    TCI_HAL_Free(client->options.username);
    TCI_HAL_Free(client->options.password);
    TCI_HAL_MutexDestroy(client->lock_generic);
    TCI_HAL_MutexDestroy(client->lock_write_buf);
    TCI_HAL_MutexDestroy(client->lock_yield);
    qcloud_iot_mqtt_sub_handle_array_clear(client);

    qcloud_iot_mqtt_suback_wait_list_clear(client);
    utils_list_destroy(client->list_pub_wait_ack);
    utils_list_destroy(client->list_sub_wait_ack);
    Log_i("release mqtt client resources");
}

/**************************************************************************************
 * API
 **************************************************************************************/

/**
 * @brief Create MQTT client and connect to MQTT server.
 *
 * @param[in] params MQTT init parameters
 * @return a valid MQTT client handle when success, or NULL otherwise
 */
void* TCIOT_MQTT_Construct(const MQTTInitParams* params)
{
    POINTER_SANITY_CHECK(params, NULL);
    POINTER_SANITY_CHECK(params->device_info, NULL);

    int rc = 0;

    QcloudIotClient* client = NULL;

    // create and init MQTTClient
    client = (QcloudIotClient*)TCI_HAL_Malloc(sizeof(QcloudIotClient));
    if (!client) {
        Log_e("malloc MQTTClient failed");
        return NULL;
    }

    rc = _qcloud_iot_mqtt_client_init(client, params);
    if (rc) {
        Log_e("mqtt init failed: %d", rc);
        goto exit;
    }

    if (!params->connect_when_construct) {
        return client;
    }

    rc = qcloud_iot_mqtt_connect(client, FIRST_CONNECT);
    if (rc) {
        Log_e("mqtt connect with id: %s failed: %d", STRING_PTR_PRINT_SANITY_CHECK(client->conn_id), rc);
        goto exit;
    }

    Log_i("mqtt connect with id: %s success", client->conn_id);

    // Create yield thread
    static TCI_ThreadParams thread_params = {0};
    thread_params.thread_func             = _mqtt_yield_thread;
    thread_params.thread_name             = "_mqtt_yield_thread";
    thread_params.user_arg                = client;
    thread_params.stack_size              = QCLOUD_TCIOT_MQTT_RX_BUF_LEN * 2;
    thread_params.priority                = TCI_THREAD_PRIORITY_NORMAL;
    client->yield_thread_running          = 1;

    rc = TCI_HAL_ThreadCreate(&thread_params);
    if (rc) {
        Log_e("create mqtt yield thread fail: %d", rc);
        qcloud_iot_mqtt_disconnect(client);
        goto exit;
    }
    Log_i("mqtt yield thread created");

    return client;
exit:
    _qcloud_iot_mqtt_client_deinit(client);
    TCI_HAL_Free(client);
    return NULL;
}

/**
 * @brief Connect Mqtt server if not connect.
 *
 * @param[in,out] client pointer to mqtt client pointer, should using the pointer of TCIOT_MQTT_Construct return.
 * @return @see IotReturnCode
 */
int TCIOT_MQTT_Connect(void* client)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    if (!get_client_conn_state(client)) {
        TCIOT_FUNC_EXIT_RC(qcloud_iot_mqtt_connect(client, FIRST_CONNECT));
    }
    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
}

/**
 * @brief Close connection and destroy MQTT client.
 *
 * @param[in,out] client pointer to mqtt client pointer, should using the pointer of TCIOT_MQTT_Construct return.
 * @return @see IotReturnCode
 */
int TCIOT_MQTT_Destroy(void** client)
{
    POINTER_SANITY_CHECK(*client, QCLOUD_ERR_INVAL);

    QcloudIotClient* mqtt_client = (QcloudIotClient*)(*client);

    // Stop yield thread
    mqtt_client->yield_thread_running = 0;
    int cnt                           = 0;
    do {
        TCI_HAL_SleepMs(100);
        cnt++;
    } while ((!mqtt_client->yield_thread_exit) && (cnt < 180));
    Log_i("mqtt yield thread stopped");

    int rc = qcloud_iot_mqtt_disconnect(mqtt_client);
    if (rc) {
        // disconnect network stack by force
        mqtt_client->network_stack.disconnect(&(mqtt_client->network_stack));
        set_client_conn_state(mqtt_client, NOTCONNECTED);
    }

    _qcloud_iot_mqtt_client_deinit(mqtt_client);

    TCI_HAL_Free(*client);
    *client = NULL;
    Log_i("mqtt release!");
    return rc;
}

/**
 * @brief Check connection and keep alive state, read/handle MQTT packet in synchronized way.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] timeout_ms timeout value (unit: ms) for this operation
 * @return QCLOUD_RET_SUCCESS when success, QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT when try reconnecting, others @see
 * IotReturnCode
 */
int TCIOT_MQTT_Yield(void* client, uint32_t timeout_ms)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;

    /* only one instance of yield is allowed in running state*/
    if (mqtt_client->yield_thread_running) {
        TCI_HAL_SleepMs(timeout_ms);
        return QCLOUD_RET_SUCCESS;
    }

    return qcloud_iot_mqtt_yield(mqtt_client, timeout_ms);
}

/**
 * @brief Publish MQTT message.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_name topic to publish
 * @param[in] params @see PublishParams
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int TCIOT_MQTT_Publish(void* client, const char* topic_name, const PublishParams* params)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(params, QCLOUD_ERR_INVAL);
    STRING_PTR_SANITY_CHECK(topic_name, QCLOUD_ERR_INVAL);

    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;

    if (!get_client_conn_state(client)) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_NO_CONN);
    }

    if (strlen(topic_name) > MAX_SIZE_OF_CLOUD_TOPIC) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MAX_TOPIC_LENGTH);
    }

    if (QOS2 == params->qos) {
        Log_e("QoS2 is not supported currently");
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_QOS_NOT_SUPPORT);
    }

    return qcloud_iot_mqtt_publish(mqtt_client, topic_name, params);
}

/**
 * @brief Subscribe MQTT topic.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter to subscribe
 * @param[in] params @see SubscribeParams
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int TCIOT_MQTT_Subscribe(void* client, const char* topic_filter, const SubscribeParams* params)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(params, QCLOUD_ERR_INVAL);
    STRING_PTR_SANITY_CHECK(topic_filter, QCLOUD_ERR_INVAL);

    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;

    if (!get_client_conn_state(client) && !mqtt_client->default_subscribe) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_NO_CONN);
    }

    if (strlen(topic_filter) > MAX_SIZE_OF_CLOUD_TOPIC) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MAX_TOPIC_LENGTH);
    }

    if (QOS2 == params->qos) {
        Log_e("QoS2 is not supported currently");
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_QOS_NOT_SUPPORT);
    }

    return qcloud_iot_mqtt_subscribe(mqtt_client, topic_filter, params);
}

/**
 * @brief Unsubscribe MQTT topic.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter to unsubscribe
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int TCIOT_MQTT_Unsubscribe(void* client, const char* topic_filter)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    STRING_PTR_SANITY_CHECK(topic_filter, QCLOUD_ERR_INVAL);

    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;

    if (!get_client_conn_state(client)) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_NO_CONN);
    }

    if (strlen(topic_filter) > MAX_SIZE_OF_CLOUD_TOPIC) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MAX_TOPIC_LENGTH);
    }

    return qcloud_iot_mqtt_unsubscribe(mqtt_client, topic_filter);
}

/**
 * @brief Check if MQTT topic has been subscribed or not
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter to subscribe
 * @return TCIOT_BOOL_TRUE already subscribed
 * @return TCIOT_BOOL_FALSE not ready
 */
IotBool TCIOT_MQTT_IsSubReady(void* client, const char* topic_filter)
{
    POINTER_SANITY_CHECK(client, TCIOT_BOOL_FALSE);
    STRING_PTR_SANITY_CHECK(topic_filter, TCIOT_BOOL_FALSE);

    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;
    return qcloud_iot_mqtt_is_sub_ready(mqtt_client, topic_filter);
}

/**
 * @brief Get user data in subscribe.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter to subscribe
 * @return NULL or user data
 */
void* TCIOT_MQTT_GetSubUsrData(void* client, const char* topic_filter)
{
    POINTER_SANITY_CHECK(client, NULL);
    STRING_PTR_SANITY_CHECK(topic_filter, NULL);

    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;
    return qcloud_iot_mqtt_get_subscribe_usr_data(mqtt_client, topic_filter);
}

/**
 * @brief Subscribe and wait sub ready.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter to subscribe
 * @param[in] params @see SubscribeParams
 * @return @see IotReturnCode
 */
int TCIOT_MQTT_SubscribeSync(void* client, const char* topic_filter, const SubscribeParams* params)
{
    POINTER_SANITY_CHECK(client, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(params, QCLOUD_ERR_INVAL);
    STRING_PTR_SANITY_CHECK(topic_filter, QCLOUD_ERR_INVAL);

    int rc;

    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;

    int cnt_sub = mqtt_client->command_timeout_ms / QCLOUD_TCIOT_MQTT_YIELD_TIMEOUT;

    if (TCIOT_MQTT_IsSubReady(client, topic_filter)) {
        // if already sub, free the user data
        if (params->user_data_free) {
            params->user_data_free(params->user_data);
        }
        return QCLOUD_RET_SUCCESS;
    }

    rc = TCIOT_MQTT_Subscribe(client, topic_filter, params);
    if (rc < 0) {
        Log_e("topic subscribe failed: %d, cnt: %d", rc, cnt_sub);
        return rc;
    }

    while (cnt_sub-- >= 0 && rc >= 0 && !TCIOT_MQTT_IsSubReady(client, topic_filter)) {
        /**
         * @brief wait for subscription result
         *
         */
        rc = TCIOT_MQTT_Yield(client, QCLOUD_TCIOT_MQTT_YIELD_TIMEOUT);
    }
    return TCIOT_MQTT_IsSubReady(client, topic_filter) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

/**
 * @brief Check if MQTT is connected.
 *
 * @param[in,out] client pointer to mqtt client
 * @return TCIOT_BOOL_TRUE connected
 * @return TCIOT_BOOL_FALSE no connected
 */
IotBool TCIOT_MQTT_IsConnected(void* client)
{
    POINTER_SANITY_CHECK(client, TCIOT_BOOL_FALSE);
    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;
    return get_client_conn_state(mqtt_client);
}

/**
 * @brief Set trustee used in services.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] trustee trustee device
 */
void TCIOT_MQTT_Proxy(void* client, DeviceInfo* trustee)
{
    POINTER_SANITY_CHECK_RTN(client);
    QcloudIotClient* mqtt_client     = (QcloudIotClient*)client;
    mqtt_client->trustee_device_info = trustee;
    mqtt_client->default_subscribe   = trustee ? 1 : 0;
    return;
}

/**
 * @brief Get device info used in services.
 *
 * @param[in,out] client pointer to mqtt client
 * @return @see DeviceInfo
 */
DeviceInfo* TCIOT_MQTT_GetDeviceInfo(void* client)
{
    POINTER_SANITY_CHECK(client, NULL);
    QcloudIotClient* mqtt_client = (QcloudIotClient*)client;
    return mqtt_client->trustee_device_info ? mqtt_client->trustee_device_info : mqtt_client->device_info;
}
