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
 * @file mqtt_client_subscribe.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-05-28
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-28 <td>1.0     <td>fancyxu   <td>first commit
 * <tr><td>2021-07-08 <td>1.1     <td>fancyxu   <td>fix code standard of IotReturnCode and QcloudIotClient
 * </table>
 */

#include "mqtt_client.h"

/**
 * @brief Context of subscribe
 *
 */
typedef struct {
    uint16_t         packet_id;
    char            *topic;
    SubscribeParams *params;
} QcloudIotSubContext;

/**
 * @brief Free topic_filter
 *
 * @param[in] handler subtopic handle
 */
static void _clear_sub_handle(SubTopicHandle *handler)
{
    if (handler->topic_filter) {
        TCI_HAL_Free(handler->topic_filter);
        handler->topic_filter = NULL;
    }
}

/**
 * @brief Free user_data
 *
 * @param[in] handler subtopic handle
 */
static void _clear_sub_handle_usr_data(SubTopicHandle *handler)
{
    if (handler->params.user_data && handler->params.user_data_free) {
        handler->params.user_data_free(handler->params.user_data);
        handler->params.user_data = NULL;
    }
}

typedef struct {
    QcloudIotClient       *client;
    uint16_t               packet_id;
    MQTTPacketType         type;
    const char            *topic_filter;
    const SubscribeParams *params;
    void                 **sub_info_val;
} PushSubInfo;

static int _construct_sub_info(void *val, void *usr_data, size_t size)
{
    QcloudIotSubInfo *sub_info      = val;
    PushSubInfo      *push_sub_info = usr_data;

    memset(sub_info, 0, sizeof(QcloudIotSubInfo));
    sub_info->type      = push_sub_info->type;
    sub_info->packet_id = push_sub_info->packet_id;
    if (push_sub_info->params) {
        sub_info->params = *(push_sub_info->params);
    }
    size_t topic_len = strlen(push_sub_info->topic_filter);
    strncpy(sub_info->topic, push_sub_info->topic_filter, topic_len);
    sub_info->topic[topic_len] = '\0';

    TCI_HAL_TimerCountdownMs(&sub_info->sub_start_time, push_sub_info->client->command_timeout_ms);
    *(push_sub_info->sub_info_val) = sub_info;
    return 0;
}

/**
 * @brief Push node to subscribe(unsubscribe) ACK wait list.
 *
 * @param[in,out] client pointer to mqtt_client
 * @param[in] packet_id packet id
 * @param[in] type mqtt packet type SUBSCRIBE or UNSUBSCRIBE
 * @param[in] topic_filter subtopic
 * @param[in] params params
 * @param[out] sub_info_val sub_info to push to list
 * @return @see IotReturnCode
 */
static int _push_sub_info_to_list(PushSubInfo *push_sub_info)
{
    TCIOT_FUNC_ENTRY;
    void *list = push_sub_info->client->list_sub_wait_ack;
    if (utils_list_push(list, sizeof(QcloudIotSubInfo), push_sub_info, _construct_sub_info)) {
        Log_e("list push failed! Check the list len!");
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_FAILURE);
    }
    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
}

/**
 * @brief Check pub wait list timeout.
 *
 * @param[in,out] list pointer to pub wait ack list.
 * @param[in] node pointer to list node
 * @param[in] val pointer to value, @see QcloudIotPubInfo
 * @param[in] usr_data @see QcloudIotClient
 * @return @see UtilsListResult
 */
static UtilsListResult _sub_wait_list_process_pop_info(void *list, void *val, void *usr_data)
{
    QcloudIotSubContext *sub_context = (QcloudIotSubContext *)usr_data;
    QcloudIotSubInfo    *sub_info    = (QcloudIotSubInfo *)val;

    if (sub_info->packet_id == sub_context->packet_id) {
        if (sub_context->topic) {
            strncpy(sub_context->topic, sub_info->topic, MAX_SIZE_OF_CLOUD_TOPIC);
        }
        if (sub_context->params) {
            *(sub_context->params) = sub_info->params;
        }
        utils_list_remove(list, val);
        return LIST_TRAVERSE_BREAK;
    }
    return LIST_TRAVERSE_CONTINUE;
}

/**
 * @brief Check sub wait list timeout.
 *
 * @param[in,out] list pointer to sub wait ack list.
 * @param[in] node pointer to list node
 * @param[in] val pointer to value, @see QcloudIotSubInfo
 * @param[in] usr_data @see QcloudIotClient
 * @return @see UtilsListResult
 */
static UtilsListResult _sub_wait_list_process_check_timeout(void *list, void *val, void *usr_data)
{
    MQTTEventMsg      msg;
    QcloudIotSubInfo *sub_info = (QcloudIotSubInfo *)val;
    QcloudIotClient  *client   = (QcloudIotClient *)usr_data;

    // check the request if timeout or not
    if (TCI_HAL_TimerRemain(&sub_info->sub_start_time) > 0) {
        return LIST_TRAVERSE_CONTINUE;
    }

    // notify timeout event
    if (client->event_handle.h_fp) {
        msg.event_type = SUBSCRIBE == sub_info->type ? MQTT_EVENT_SUBSCRIBE_TIMEOUT : MQTT_EVENT_UNSUBSCRIBE_TIMEOUT;
        msg.msg        = (void *)(uintptr_t)sub_info->packet_id;
        if (sub_info->params.on_sub_event_handler) {
            sub_info->params.on_sub_event_handler(client, MQTT_EVENT_SUBSCRIBE_TIMEOUT, sub_info->params.user_data);
        }
        client->event_handle.h_fp(client, client->event_handle.context, &msg);
    }

    utils_list_remove(list, val);
    return LIST_TRAVERSE_CONTINUE;
}

/**
 * @brief Clear sub wait list.
 *
 * @param[in,out] list pointer to sub wait list.
 * @param[in] node pointer to list node
 * @param[in] val pointer to value, @see QcloudIotSubInfo
 * @param[in] usr_data null
 * @return @see UtilsListResult
 */
static UtilsListResult _sub_wait_list_process_clear(void *list, void *val, void *usr_data)
{
    utils_list_remove(list, val);
    return LIST_TRAVERSE_CONTINUE;
}

/**
 * @brief Pop node signed with packet id from subscribe ACK wait list, and return the sub handler
 *
 * @param[in,out] client pointer to mqtt_client
 * @param[in] packet_id packet id
 * @param[out] topic sub topic
 * @param[out] params params
 */
static void _pop_sub_info_from_list(QcloudIotClient *client, uint16_t packet_id, char *topic, SubscribeParams *params)
{
    QcloudIotSubContext sub_context = {.packet_id = packet_id, .topic = topic, .params = params};
    utils_list_process(client->list_sub_wait_ack, LIST_HEAD, _sub_wait_list_process_pop_info, &sub_context);
}

/**
 * @brief Remove sub handle when unsubscribe.
 *
 * @param[in,out] client pointer to mqtt_client
 * @param[in] topic_filter topic to remove
 * @param[in] is_unsub is unsubcribe
 * @return TCIOT_BOOL_TRUE topic exist
 * @return TCIOT_BOOL_FALSE topic no exist
 */
static IotBool _remove_sub_handle_from_array(QcloudIotClient *client, const char *topic_filter, int is_unsub)
{
    int     i;
    IotBool topic_exists = TCIOT_BOOL_FALSE;

    // remove from message handler array
    TCI_HAL_MutexLock(client->lock_generic);
    for (i = 0; i < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; ++i) {
        if ((client->sub_handles[i].topic_filter && !strcmp(client->sub_handles[i].topic_filter, topic_filter)) ||
            strstr(topic_filter, "/#") || strstr(topic_filter, "/+")) {
            // notify this event to topic subscriber
            if (client->sub_handles[i].params.on_sub_event_handler) {
                client->sub_handles[i].params.on_sub_event_handler(client, MQTT_EVENT_UNSUBSCRIBE,
                                                                   client->sub_handles[i].params.user_data);
            }
            _clear_sub_handle(&client->sub_handles[i]);
            if (is_unsub) {
                _clear_sub_handle_usr_data(&client->sub_handles[i]);
            }
            // we don't want to break here, if the same topic is registered*with 2 callbacks.Unlikely scenario
            topic_exists = TCIOT_BOOL_TRUE;
        }
    }
    TCI_HAL_MutexUnlock(client->lock_generic);
    return topic_exists;
}

/**
 * @brief Add sub handle when subscribe.
 *
 * @param[in,out] client pointer to mqtt_client
 * @param[in] sub_handle sub_handle to be add to array
 * @return TCIOT_BOOL_TRUE topic exist
 * @return TCIOT_BOOL_FALSE topic no exist
 */
static int _add_sub_handle_to_array(QcloudIotClient *client, const SubTopicHandle *sub_handle)
{
    TCIOT_FUNC_ENTRY;
    int i, i_free = -1;

    TCI_HAL_MutexLock(client->lock_generic);

    for (i = 0; i < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; ++i) {
        if (client->sub_handles[i].topic_filter) {
            if (!strcmp(client->sub_handles[i].topic_filter, sub_handle->topic_filter)) {
                i_free = i;
                // free the memory before
                _clear_sub_handle(&client->sub_handles[i]);
                _clear_sub_handle_usr_data(&client->sub_handles[i]);
                Log_w("Identical topic found: %s", sub_handle->topic_filter);
                break;
            }
        } else {
            i_free = i_free == -1 ? i : i_free;
        }
    }

    if (-1 == i_free) {
        Log_e("NO more @sub_handles space!");
        TCI_HAL_MutexUnlock(client->lock_generic);
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_FAILURE);
    }
    client->sub_handles[i_free] = *sub_handle;
    TCI_HAL_MutexUnlock(client->lock_generic);
    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
}

/**
 * @brief Set sub handle status.
 *
 * @param[in,out] client pointer to mqtt_client
 * @param[in] topic_filter topic to set status
 * @param[in] status @see SubStatus
 */
static void _set_sub_handle_status_to_array(QcloudIotClient *client, const char *topic_filter, SubStatus status)
{
    TCIOT_FUNC_ENTRY;
    int i;

    TCI_HAL_MutexLock(client->lock_generic);

    for (i = 0; i < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; ++i) {
        if (client->sub_handles[i].topic_filter) {
            if (!strcmp(client->sub_handles[i].topic_filter, topic_filter)) {
                client->sub_handles[i].status = status;
                break;
            }
        }
    }
    TCI_HAL_MutexUnlock(client->lock_generic);
    TCIOT_FUNC_EXIT;
}

/**
 * @brief Serialize and send subscribe packet.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic to subscribe
 * @param[in] params subscribe params
 * @return >=0 for packet id, < 0 for failed @see IotReturnCode
 */
int qcloud_iot_mqtt_subscribe(QcloudIotClient *client, const char *topic_filter, const SubscribeParams *params)
{
    TCIOT_FUNC_ENTRY;
    int            rc, packet_len, qos = params->qos;
    uint16_t       packet_id;
    char          *topic_filter_stored;
    void          *sub_info = NULL;
    SubTopicHandle sub_handle;

    // topic filter should be valid in the whole sub life
    topic_filter_stored = TCI_HAL_Malloc(strlen(topic_filter) + 1);
    if (!topic_filter_stored) {
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MALLOC);
    }
    strncpy(topic_filter_stored, topic_filter, strlen(topic_filter) + 1);

    sub_handle.topic_filter = topic_filter_stored;
    sub_handle.params       = *params;
    sub_handle.status       = client->default_subscribe ? SUB_ACK_RECEIVED : SUB_ACK_NOT_RECEIVED;

    // add sub handle first to process
    rc = _add_sub_handle_to_array(client, &sub_handle);
    if (rc) {
        goto exit;
    }

    if (client->default_subscribe) {
        return 0;
    }

    packet_id = get_next_packet_id(client);
    Log_d("subscribe topic_name=%s|packet_id=%d", topic_filter_stored, packet_id);
    // serialize packet
    TCI_HAL_MutexLock(client->lock_write_buf);
    packet_len =
        mqtt_subscribe_packet_serialize(client->write_buf, client->write_buf_size, packet_id, 1, &topic_filter, &qos);
    if (packet_len < 0) {
        TCI_HAL_MutexUnlock(client->lock_write_buf);
        rc = packet_len == MQTT_ERR_SHORT_BUFFER ? QCLOUD_ERR_BUF_TOO_SHORT : QCLOUD_ERR_FAILURE;
        goto exit;
    }

    // add node into sub ack wait list
    PushSubInfo push_sub_info = {client, packet_id, SUBSCRIBE, topic_filter, params, &sub_info};

    rc = _push_sub_info_to_list(&push_sub_info);
    if (rc) {
        TCI_HAL_MutexUnlock(client->lock_write_buf);
        goto exit;
    }

    // send packet
    rc = send_mqtt_packet(client, packet_len);
    TCI_HAL_MutexUnlock(client->lock_write_buf);
    if (rc) {
        utils_list_remove(client->list_sub_wait_ack, sub_info);
        goto exit;
    }
    TCIOT_FUNC_EXIT_RC(packet_id);
exit:
    _remove_sub_handle_from_array(client, topic_filter_stored, 0);
    TCIOT_FUNC_EXIT_RC(rc);
}

/**
 * @brief Deserialize suback packet and return sub result.
 *
 * @param[in,out] client pointer to mqtt client
 * @return @see IotReturnCode
 */
int qcloud_iot_mqtt_handle_suback(QcloudIotClient *client)
{
    TCIOT_FUNC_ENTRY;
    int           rc, count = 0;
    uint16_t      packet_id = 0;
    int           granted_qos;
    MQTTEventMsg  msg        = {MQTT_EVENT_SUBSCRIBE_SUCCESS, NULL};
    MQTTEventType event_type = MQTT_EVENT_SUBSCRIBE_SUCCESS;

    rc = mqtt_suback_packet_deserialize(client->read_buf, client->read_buf_size, 1, &count, &packet_id, &granted_qos);
    if (rc) {
        TCIOT_FUNC_EXIT_RC(rc);
    }
    msg.msg = (void *)(uintptr_t)packet_id;

    // pop sub info and get sub handle
    char            topic[MAX_SIZE_OF_CLOUD_TOPIC] = {0};
    SubscribeParams params                         = {0};
    _pop_sub_info_from_list(client, packet_id, topic, &params);
    if (!topic[0]) {
        Log_w("can't get sub handle from list!");
        TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);  // in case of resubscribe when reconnect
    }

    // check return code in SUBACK packet: 0x00(QOS0, SUCCESS),0x01(QOS1, SUCCESS),0x02(QOS2,SUCCESS),0x80(Failure)
    if (0x80 == granted_qos) {
        msg.event_type = MQTT_EVENT_SUBSCRIBE_NACK;
        event_type     = MQTT_EVENT_SUBSCRIBE_NACK;
        Log_e("MQTT SUBSCRIBE failed, packet_id: %u topic: %s", packet_id, topic);
        _remove_sub_handle_from_array(client, topic, 0);
        rc = QCLOUD_ERR_MQTT_SUB;
    }

    if (MQTT_EVENT_SUBSCRIBE_SUCCESS == msg.event_type) {
        _set_sub_handle_status_to_array(client, topic, SUB_ACK_RECEIVED);
    }

    // notify this event to user callback
    if (client->event_handle.h_fp) {
        client->event_handle.h_fp(client, client->event_handle.context, &msg);
    }

    // notify this event to topic subscriber
    if (params.on_sub_event_handler) {
        params.on_sub_event_handler(client, event_type, params.user_data);
    }
    TCIOT_FUNC_EXIT_RC(rc);
}

/**
 * @brief Serialize and send unsubscribe packet.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic to unsubscribe
 * @return >=0 packet id, < 0 for failed @see IotReturnCode
 */
int qcloud_iot_mqtt_unsubscribe(QcloudIotClient *client, const char *topic_filter)
{
    TCIOT_FUNC_ENTRY;
    int      rc, packet_len;
    uint16_t packet_id;

    void          *sub_info_val = NULL;
    SubTopicHandle sub_handle;
    memset(&sub_handle, 0, sizeof(SubTopicHandle));

    // remove from sub handle
    if (!_remove_sub_handle_from_array(client, topic_filter, 1)) {
        Log_w("subscription does not exists: %s", topic_filter);
        TCIOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_UNSUB_FAIL);
    }

    if (client->default_subscribe) {
        return QCLOUD_RET_SUCCESS;
    }

    packet_id = get_next_packet_id(client);
    Log_d("unsubscribe topic_name=%s|packet_id=%d", topic_filter, packet_id);

    TCI_HAL_MutexLock(client->lock_write_buf);
    packet_len =
        mqtt_unsubscribe_packet_serialize(client->write_buf, client->write_buf_size, packet_id, 1, &topic_filter);
    if (packet_len < 0) {
        TCI_HAL_MutexUnlock(client->lock_write_buf);
        rc = packet_len == MQTT_ERR_SHORT_BUFFER ? QCLOUD_ERR_BUF_TOO_SHORT : QCLOUD_ERR_FAILURE;
        goto exit;
    }

    // add node into sub ack wait list
    PushSubInfo push_sub_info = {client, packet_id, UNSUBSCRIBE, topic_filter, NULL, &sub_info_val};

    rc = _push_sub_info_to_list(&push_sub_info);
    if (rc) {
        Log_e("push unsubscribe info failed!");
        TCI_HAL_MutexUnlock(client->lock_write_buf);
        goto exit;
    }

    /* send the unsubscribe packet */
    rc = send_mqtt_packet(client, packet_len);
    TCI_HAL_MutexUnlock(client->lock_write_buf);
    if (rc) {
        utils_list_remove(client->list_sub_wait_ack, sub_info_val);
        goto exit;
    }

    TCIOT_FUNC_EXIT_RC(packet_id);
exit:
    _clear_sub_handle(&sub_handle);
    TCIOT_FUNC_EXIT_RC(rc);
}

/**
 * @brief Deserialize unsuback packet and remove from list.
 *
 * @param[in,out] client pointer to mqtt client
 * @return @see IotReturnCode
 */
int qcloud_iot_mqtt_handle_unsuback(QcloudIotClient *client)
{
    TCIOT_FUNC_ENTRY;
    int          rc;
    uint16_t     packet_id = 0;
    MQTTEventMsg msg;

    rc = mqtt_unsuback_packet_deserialize(client->read_buf, client->read_buf_size, &packet_id);
    if (rc) {
        TCIOT_FUNC_EXIT_RC(rc);
    }

    _pop_sub_info_from_list(client, packet_id, NULL, NULL);

    if (client->event_handle.h_fp) {
        msg.event_type = MQTT_EVENT_UNSUBSCRIBE_SUCCESS;
        msg.msg        = (void *)(uintptr_t)packet_id;
        client->event_handle.h_fp(client, client->event_handle.context, &msg);
    }

    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
}

/**
 * @brief Process suback waiting timeout.
 *
 * @param[in,out] client pointer to mqtt client
 */
void qcloud_iot_mqtt_check_sub_timeout(QcloudIotClient *client)
{
    TCIOT_FUNC_ENTRY;
    utils_list_process(client->list_sub_wait_ack, LIST_HEAD, _sub_wait_list_process_check_timeout, client);
    TCIOT_FUNC_EXIT;
}

/**
 * @brief Resubscribe topic when reconnect.
 *
 * @param[in,out] client pointer to mqtt client
 * @return @see IotReturnCode
 */
int qcloud_iot_mqtt_resubscribe(QcloudIotClient *client)
{
    TCIOT_FUNC_ENTRY;
    int             rc, packet_len, qos, itr = 0;
    uint16_t        packet_id;
    SubTopicHandle *sub_handle;

    for (itr = 0; itr < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; itr++) {
        sub_handle = &client->sub_handles[itr];
        if (!sub_handle->topic_filter) {
            continue;
        }

        packet_id = get_next_packet_id(client);
        Log_d("subscribe topic_name=%s|packet_id=%d", sub_handle->topic_filter, packet_id);

        TCI_HAL_MutexLock(client->lock_write_buf);
        qos        = sub_handle->params.qos;
        packet_len = mqtt_subscribe_packet_serialize(client->write_buf, client->write_buf_size, packet_id, 1,
                                                     (const char **)&sub_handle->topic_filter, &qos);
        if (packet_len < 0) {
            TCI_HAL_MutexUnlock(client->lock_write_buf);
            continue;
        }

        // send packet
        rc = send_mqtt_packet(client, packet_len);
        TCI_HAL_MutexUnlock(client->lock_write_buf);
        if (rc) {
            TCIOT_FUNC_EXIT_RC(rc);
        }
    }

    TCIOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
}

/**
 * @brief Return if topic is sub ready.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter
 * @return TCIOT_BOOL_TRUE for ready
 * @return TCIOT_BOOL_FALSE for not ready
 */
IotBool qcloud_iot_mqtt_is_sub_ready(QcloudIotClient *client, const char *topic_filter)
{
    TCIOT_FUNC_ENTRY;
    int i = 0;
    TCI_HAL_MutexLock(client->lock_generic);
    for (i = 0; i < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; ++i) {
        if ((client->sub_handles[i].topic_filter && !strcmp(client->sub_handles[i].topic_filter, topic_filter)) ||
            strstr(topic_filter, "/#") || strstr(topic_filter, "/+")) {
            TCI_HAL_MutexUnlock(client->lock_generic);
            return client->sub_handles[i].status == SUB_ACK_RECEIVED;
        }
    }
    TCI_HAL_MutexUnlock(client->lock_generic);
    TCIOT_FUNC_EXIT_RC(TCIOT_BOOL_FALSE);
}

/**
 * @brief Get usr data, usr should handle lock/unlock usrdata itself in callback and caller.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] topic_filter topic filter
 * @return NULL or user data
 */
void *qcloud_iot_mqtt_get_subscribe_usr_data(QcloudIotClient *client, const char *topic_filter)
{
    TCIOT_FUNC_ENTRY;
    int i = 0;
    TCI_HAL_MutexLock(client->lock_generic);
    for (i = 0; i < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; ++i) {
        if ((client->sub_handles[i].topic_filter && !strcmp(client->sub_handles[i].topic_filter, topic_filter)) ||
            strstr(topic_filter, "/#") || strstr(topic_filter, "/+")) {
            TCI_HAL_MutexUnlock(client->lock_generic);
            return client->sub_handles[i].params.user_data;
        }
    }
    TCI_HAL_MutexUnlock(client->lock_generic);
    TCIOT_FUNC_EXIT_RC(NULL);
}

/**
 * @brief Clear sub handle array.
 *
 * @param[in,out] client pointer to mqtt client
 */
void qcloud_iot_mqtt_sub_handle_array_clear(QcloudIotClient *client)
{
    TCIOT_FUNC_ENTRY;
    int i;
    for (i = 0; i < QCLOUD_TCIOT_MQTT_MAX_MESSAGE_HANDLERS; ++i) {
        /* notify this event to topic subscriber */
        if (client->sub_handles[i].topic_filter && client->sub_handles[i].params.on_sub_event_handler) {
            client->sub_handles[i].params.on_sub_event_handler(client, MQTT_EVENT_CLIENT_DESTROY,
                                                               client->sub_handles[i].params.user_data);
        }
        _clear_sub_handle(&client->sub_handles[i]);
        _clear_sub_handle_usr_data(&client->sub_handles[i]);
    }

    TCIOT_FUNC_EXIT;
}

/**
 * @brief Clear suback wait list and clear sub handle.
 *
 * @param[in,out] client pointer to mqtt client
 */
void qcloud_iot_mqtt_suback_wait_list_clear(QcloudIotClient *client)
{
    TCIOT_FUNC_ENTRY;
    utils_list_process(client->list_sub_wait_ack, LIST_HEAD, _sub_wait_list_process_clear, client);
    TCIOT_FUNC_EXIT;
}
