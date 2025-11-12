/**
 * @file qcloud_iot_link.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-08-16
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
 * 2022-08-16		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_PLATFORM_LINK_INC_QCLOUD_TCIOT_LINK_H_
#define IOT_HUB_DEVICE_C_SDK_PLATFORM_LINK_INC_QCLOUD_TCIOT_LINK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <qcloud_iot_platform.h>

#define MAX_TCIOT_LINK_ADDR_LEN 6

typedef enum {
    TCIOT_LINK_PROTOCOL_UNKOWN   = -1,
    TCIOT_LINK_PROTOCOL_BLE_MESH = 0,
    TCIOT_LINK_PROTOCOL_PLC,
    TCIOT_LINK_PROTOCOL_UDP,
    TCIOT_LINK_PROTOCOL_BLE,
    TCIOT_LINK_PROTOCOL_TCP,
} IotLinkProtocol;

typedef struct {
    void (*connect_callback)(void *usr_data, void *addr, size_t addr_len);
    void (*disconnect_callback)(void *usr_data, void *addr, size_t addr_len);
    void (*recv_callback)(void *usr_data, void *addr, size_t addr_len, uint8_t *data, size_t data_len);
    void (*find_callback)(void *usr_data, const char *product_id, void *uuid, size_t uuid_len);
    void (*sync_mtu_callback)(void *usr_data, uint16_t mtu);
} IotLinkCallback;

typedef struct {
    IotLinkProtocol protocol;
    size_t          max_node_num;
    void *(*init)(IotLinkCallback callback, void *usr_data);
    void (*deinit)(void *handle);
    int (*yield)(void *handle, uint32_t timeout_ms);
    int (*send)(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len, uint32_t timeout_ms);
    void (*disconnect)(void *handle, void *addr, size_t addr_len);
    IotBool (*match)(void *handle, const void *dev_addr, size_t dev_addr_len, const void *msg_addr,
                     size_t msg_addr_len);
    int (*invite)(void *handle, void *uuid, size_t uuid_len);
    int (*start_scan)(void *handle, IotBool start, uint32_t timeout_ms);
    int (*get_uuid)(void *handle, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len);
    int (*get_device_info)(void *handle, void *addr, size_t addr_len, DeviceInfo *dev_info);
    int (*start_advertising)(void *handle, void *adv_data, size_t adv_data_len, uint32_t adv_time);
    int (*stop_advertising)(void *handle);
    int (*device_describe_sync)(void *handle, void *cloud_dev_list, void *link);
} IotLinkNetwork;

typedef struct {
    IotLinkNetwork  network;
    IotLinkCallback callback;
    void           *usr_data;
} IotLinkInitParams;

#define TCIOT_LINK_NETWORK_NONE                                                             \
    {                                                                                     \
        .callback = {NULL, NULL, NULL, NULL, NULL}, .network = {TCIOT_LINK_PROTOCOL_UNKOWN, \
                                                                0,                        \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL,                     \
                                                                NULL},                    \
        .usr_data = NULL                                                                  \
    }

// ------------------------------------------------------------------------------------
// tcp client
// ------------------------------------------------------------------------------------
void   *qcloud_iot_link_tcp_client_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_tcp_client_deinit(void *handle);
int     qcloud_iot_link_tcp_client_yield(void *handle, uint32_t timeout_ms);
int     qcloud_iot_link_tcp_client_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                        uint32_t timeout_ms);
void    qcloud_iot_link_tcp_client_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_tcp_client_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                         size_t addr1_len);
#define IOT_LINK_NETWORK_TCP_CLIENT                                                                                   \
    {                                                                                                                 \
        TCIOT_LINK_PROTOCOL_TCP, 1, qcloud_iot_link_tcp_client_init, qcloud_iot_link_tcp_client_deinit,                 \
            qcloud_iot_link_tcp_client_yield, qcloud_iot_link_tcp_client_send, qcloud_iot_link_tcp_client_disconnect, \
            qcloud_iot_link_tcp_client_match, NULL, NULL, NULL, NULL, NULL, NULL, NULL                                \
    }

// ----------------------------------------------------------------------------------------------
// tcp server
// ----------------------------------------------------------------------------------------------
void   *qcloud_iot_link_tcp_server_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_tcp_server_deinit(void *handle);
int     qcloud_iot_link_tcp_server_yield(void *handle, uint32_t timeout_ms);
int     qcloud_iot_link_tcp_server_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                        uint32_t timeout_ms);
void    qcloud_iot_link_tcp_server_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_tcp_server_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                         size_t addr1_len);
#define IOT_LINK_NETWORK_TCP_SERVER                                                                                   \
    {                                                                                                                 \
        TCIOT_LINK_PROTOCOL_TCP, 128, qcloud_iot_link_tcp_server_init, qcloud_iot_link_tcp_server_deinit,               \
            qcloud_iot_link_tcp_server_yield, qcloud_iot_link_tcp_server_send, qcloud_iot_link_tcp_server_disconnect, \
            qcloud_iot_link_tcp_server_match, NULL, NULL, NULL, NULL, NULL, NULL, NULL                                \
    }

// ------------------------------------------------------------------------------------
// udp client
// ------------------------------------------------------------------------------------
void   *qcloud_iot_link_udp_client_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_udp_client_deinit(void *handle);
int     qcloud_iot_link_udp_client_yield(void *handle, uint32_t timeout_ms);
int     qcloud_iot_link_udp_client_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                        uint32_t timeout_ms);
void    qcloud_iot_link_udp_client_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_udp_client_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                         size_t addr1_len);

#define TCIOT_LINK_NETWORK_UDP_CLIENT                                                                                   \
    {                                                                                                                 \
        TCIOT_LINK_PROTOCOL_UDP, 1, qcloud_iot_link_udp_client_init, qcloud_iot_link_udp_client_deinit,                 \
            qcloud_iot_link_udp_client_yield, qcloud_iot_link_udp_client_send, qcloud_iot_link_udp_client_disconnect, \
            qcloud_iot_link_udp_client_match, NULL, NULL, NULL, NULL, NULL, NULL, NULL                                \
    }

// ----------------------------------------------------------------------------------------------
// udp server
// ----------------------------------------------------------------------------------------------
void   *qcloud_iot_link_udp_server_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_udp_server_deinit(void *handle);
int     qcloud_iot_link_udp_server_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                        uint32_t timeout_ms);
void    qcloud_iot_link_udp_server_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_udp_server_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                         size_t addr1_len);
int     qcloud_iot_link_udp_server_invite(void *handle, void *uuid, size_t uuid_len);
int     qcloud_iot_link_udp_server_start_scan(void *handle, IotBool start, uint32_t timeout_ms);
int     qcloud_iot_link_udp_server_get_uuid(void *handle, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len);
#define TCIOT_LINK_NETWORK_UDP_SERVER                                                                                   \
    {                                                                                                                 \
        TCIOT_LINK_PROTOCOL_UDP, 128, qcloud_iot_link_udp_server_init, qcloud_iot_link_udp_server_deinit, NULL,         \
            qcloud_iot_link_udp_server_send, qcloud_iot_link_udp_server_disconnect, qcloud_iot_link_udp_server_match, \
            qcloud_iot_link_udp_server_invite, qcloud_iot_link_udp_server_start_scan,                                 \
            qcloud_iot_link_udp_server_get_uuid, NULL, NULL, NULL, NULL                                               \
    }
// -------------------------------------------------------------------------------------
// mesh define
// -------------------------------------------------------------------------------------
#define LINK_MESH_CID_VENDOR             (0x013A)  // Tencent Identifiter
#define LINK_MESH_VENDOR_MODEL_SERVER_ID (0x0000)  // server是0x0000
#define LINK_MESH_VENDOR_MODEL_CLIENT_ID (0x0001)  // client是0x0001

#define LINK_MESH_MODEL_OP_3(b0, cid)       ((((b0) << 16) | 0xc00000) | (cid))
#define LINK_MESH_VND_MODEL_OP_LLTLV_SET    LINK_MESH_MODEL_OP_3(0x06, LINK_MESH_CID_VENDOR)
#define LINK_MESH_VND_MODEL_OP_LLTLV_STATUS LINK_MESH_MODEL_OP_3(0x07, LINK_MESH_CID_VENDOR)
#define LINK_MESH_UNICAST_ADDR_MIN          0x0020
#define LINK_MESH_UNICAST_ADDR_MAX          0x003f
// --------------------------------------------------------------------------------------
// ble mesh node
// --------------------------------------------------------------------------------------
void   *qcloud_iot_link_mesh_node_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_mesh_node_deinit(void *handle);
int     qcloud_iot_link_mesh_node_yield(void *handle, uint32_t timeout_ms);
int     qcloud_iot_link_mesh_node_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                       uint32_t timeout_ms);
void    qcloud_iot_link_mesh_node_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_mesh_node_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                        size_t addr1_len);
int     qcloud_iot_link_mesh_node_start_scan(void *handle, IotBool start, uint32_t timeout_ms);

#define IOT_LINK_NETWORK_BLE_MESH_NODE                                                                                \
    {                                                                                                                 \
        TCIOT_LINK_PROTOCOL_BLE_MESH, 1, qcloud_iot_link_mesh_node_init, qcloud_iot_link_mesh_node_deinit,              \
            qcloud_iot_link_mesh_node_yield, qcloud_iot_link_mesh_node_send, qcloud_iot_link_mesh_node_disconnect,    \
            qcloud_iot_link_mesh_node_match, NULL, qcloud_iot_link_mesh_node_start_scan, NULL, NULL, NULL, NULL, NULL \
    }

// --------------------------------------------------------------------------------------
// ble mesh gateway
// --------------------------------------------------------------------------------------
void   *qcloud_iot_link_mesh_gateway_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_mesh_gateway_deinit(void *handle);
int     qcloud_iot_link_mesh_gateway_yield(void *handle, uint32_t timeout_ms);
int     qcloud_iot_link_mesh_gateway_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                          uint32_t timeout_ms);
void    qcloud_iot_link_mesh_gateway_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_mesh_gateway_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                           size_t addr1_len);
int     qcloud_iot_link_mesh_gateway_invite(void *handle, void *uuid, size_t uuid_len);
int     qcloud_iot_link_mesh_gateway_start_scan(void *handle, IotBool start, uint32_t timeout_ms);
int qcloud_iot_link_mesh_gateway_get_uuid(void *handle, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len);
int qcloud_iot_link_mesh_gateway_get_device_info(void *handle, void *addr, size_t addr_len, DeviceInfo *dev_info);
int qcloud_iot_link_mesh_gateway_start_advertising(void *handle, void *adv_data, size_t adv_data_len,
                                                   uint32_t adv_time);
int qcloud_iot_link_mesh_gateway_stop_advertising(void *handle);
int qcloud_iot_link_mesh_gateway_device_describe_sync(void *handle, void *cloud_dev_list, void *link);

#define IOT_LINK_NETWORK_BLE_MESH_GATEWAY                                                                       \
    {                                                                                                           \
        TCIOT_LINK_PROTOCOL_BLE_MESH, 32, qcloud_iot_link_mesh_gateway_init, qcloud_iot_link_mesh_gateway_deinit, \
            qcloud_iot_link_mesh_gateway_yield, qcloud_iot_link_mesh_gateway_send,                              \
            qcloud_iot_link_mesh_gateway_disconnect, qcloud_iot_link_mesh_gateway_match,                        \
            qcloud_iot_link_mesh_gateway_invite, qcloud_iot_link_mesh_gateway_start_scan,                       \
            qcloud_iot_link_mesh_gateway_get_uuid, qcloud_iot_link_mesh_gateway_get_device_info,                \
            qcloud_iot_link_mesh_gateway_start_advertising, qcloud_iot_link_mesh_gateway_stop_advertising,      \
            qcloud_iot_link_mesh_gateway_device_describe_sync                                                   \
    }

// --------------------------------------------------------------------------------------
// ble
// --------------------------------------------------------------------------------------
void *qcloud_iot_link_ble_init(IotLinkCallback callback, void *usr_data);
void  qcloud_iot_link_ble_deinit(void *handle);
int   qcloud_iot_link_ble_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                               uint32_t timeout_ms);
int   qcloud_iot_link_ble_get_uuid(void *handle, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len);
int   qcloud_iot_link_ble_start_advertising(void *handle, void *adv_data, size_t adv_data_len, uint32_t adv_time);
int   qcloud_iot_link_ble_stop_advertising(void *handle);

#define TCIOT_LINK_NETWORK_BLE                                                                      \
    {                                                                                             \
        TCIOT_LINK_PROTOCOL_BLE, 1, qcloud_iot_link_ble_init, qcloud_iot_link_ble_deinit, NULL,     \
            qcloud_iot_link_ble_send, NULL, NULL, NULL, NULL, qcloud_iot_link_ble_get_uuid, NULL, \
            qcloud_iot_link_ble_start_advertising, qcloud_iot_link_ble_stop_advertising, NULL     \
    }

// --------------------------------------------------------------------------------------
// plc
// --------------------------------------------------------------------------------------
void   *qcloud_iot_link_plc_gateway_init(IotLinkCallback callback, void *usr_data);
void    qcloud_iot_link_plc_gateway_deinit(void *handle);
int     qcloud_iot_link_plc_gateway_yield(void *handle, uint32_t timeout_ms);
int     qcloud_iot_link_plc_gateway_send(void *handle, void *addr, size_t addr_len, uint8_t *data, size_t data_len,
                                         uint32_t timeout_ms);
void    qcloud_iot_link_plc_gateway_disconnect(void *handle, void *addr, size_t addr_len);
IotBool qcloud_iot_link_plc_gateway_match(void *handle, const void *addr0, size_t addr0_len, const void *addr1,
                                          size_t addr1_len);
int     qcloud_iot_link_plc_gateway_invite(void *handle, void *uuid, size_t uuid_len);
int     qcloud_iot_link_plc_gateway_start_scan(void *handle, IotBool start, uint32_t timeout_ms);
int     qcloud_iot_link_plc_gateway_get_uuid(void *handle, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len);
int     qcloud_iot_link_plc_gateway_get_device_info(void *handle, void *addr, size_t addr_len, DeviceInfo *dev_info);

#define IOT_LINK_NETWORK_PLC_GATEWAY                                                                            \
    {                                                                                                           \
        TCIOT_LINK_PROTOCOL_PLC, 128, qcloud_iot_link_plc_gateway_init, qcloud_iot_link_plc_gateway_deinit,       \
            qcloud_iot_link_plc_gateway_yield, qcloud_iot_link_plc_gateway_send,                                \
            qcloud_iot_link_plc_gateway_disconnect, qcloud_iot_link_plc_gateway_match,                          \
            qcloud_iot_link_plc_gateway_invite, qcloud_iot_link_plc_gateway_start_scan,                         \
            qcloud_iot_link_plc_gateway_get_uuid, qcloud_iot_link_plc_gateway_get_device_info, NULL, NULL, NULL \
    }

// ----------------------------------------------------------------------------
// iot link
// ----------------------------------------------------------------------------
void *qcloud_iot_link_create(IotLinkInitParams *params);

int qcloud_iot_link_yield(void *link, uint32_t timeout_ms);

int qcloud_iot_link_send(void *link, void *addr, size_t addr_len, uint8_t *data, size_t data_len, uint32_t timeout_ms);

void qcloud_iot_link_disconnect(void *link, void *addr, size_t addr_len);

IotBool qcloud_iot_link_match(void *link, const void *addr0, size_t addr0_len, const void *addr1, size_t addr1_len);

void qcloud_iot_link_destroy(void *link);

int qcloud_iot_link_invite_subdev(void *link, void *uuid, size_t uuid_len);

int qcloud_iot_link_start_scan(void *link, IotBool start, uint32_t timeout_ms);

int qcloud_iot_link_get_uuid(void *link, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len);

int qcloud_iot_link_get_device_info(void *link, void *addr, size_t addr_len, DeviceInfo *dev_info);

int qcloud_iot_link_start_advertising(void *link, void *adv_data, size_t adv_data_len, uint32_t adv_time);

IotLinkProtocol qcloud_iot_link_get_protocol(void *link);

int qcloud_iot_link_stop_advertising(void *link);

int qcloud_iot_link_device_describe_sync(void *link, void *cloud_dev_list);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_PLATFORM_LINK_INC_QCLOUD_TCIOT_LINK_H_
