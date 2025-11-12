/**
 * @file qcloud_iot_link.c
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

#include "qcloud_iot_link.h"

typedef struct {
    IotLinkNetwork network;
    void          *handle;
} IotLink;

void *qcloud_iot_link_create(IotLinkInitParams *params)
{
    POINTER_SANITY_CHECK(params, NULL);
    IotLink *link = TCI_HAL_Malloc(sizeof(IotLink));
    if (!link) {
        return NULL;
    }
    link->network = params->network;
    link->handle  = params->network.init(params->callback, params->usr_data);
    if (!link->handle) {
        TCI_HAL_Free(link);
        link = NULL;
    }
    return link;
}

int qcloud_iot_link_yield(void *link, uint32_t timeout_ms)
{
    POINTER_SANITY_CHECK(link, QCLOUD_ERR_INVAL);
    IotLink *iot_link = link;
    // POINTER_SANITY_CHECK(iot_link->network.yield, QCLOUD_ERR_INVAL);
    if (!iot_link->network.yield) {
        return QCLOUD_ERR_INVAL;
    }
    return iot_link->network.yield(iot_link->handle, timeout_ms);
}

int qcloud_iot_link_send(void *link, void *addr, size_t addr_len, uint8_t *data, size_t data_len, uint32_t timeout_ms)
{
    POINTER_SANITY_CHECK(link, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(addr, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(data, QCLOUD_ERR_INVAL);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.send, QCLOUD_ERR_INVAL);
    return iot_link->network.send(iot_link->handle, addr, addr_len, data, data_len, timeout_ms);
}

void qcloud_iot_link_disconnect(void *link, void *addr, size_t addr_len)
{
    POINTER_SANITY_CHECK_RTN(link);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK_RTN(iot_link->network.disconnect);
    iot_link->network.disconnect(iot_link->handle, addr, addr_len);
}

IotBool qcloud_iot_link_match(void *link, const void *addr1, size_t addr1_len, const void *addr2, size_t addr2_len)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.match, 0);
    return iot_link->network.match(iot_link->handle, addr1, addr1_len, addr2, addr2_len);
}

int qcloud_iot_link_invite_subdev(void *link, void *uuid, size_t uuid_len)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.invite, 0);
    return iot_link->network.invite(iot_link->handle, uuid, uuid_len);
}

int qcloud_iot_link_start_scan(void *link, IotBool start, uint32_t timeout_ms)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.start_scan, 0);
    return iot_link->network.start_scan(iot_link->handle, start, timeout_ms);
}

int qcloud_iot_link_get_uuid(void *link, void *addr, size_t addr_len, uint8_t *uuid, size_t uuid_len)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.get_uuid, 0);
    return iot_link->network.get_uuid(iot_link->handle, addr, addr_len, uuid, uuid_len);
}

int qcloud_iot_link_get_device_info(void *link, void *addr, size_t addr_len, DeviceInfo *dev_info)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.get_device_info, 0);
    return iot_link->network.get_device_info(iot_link->handle, addr, addr_len, dev_info);
}

int qcloud_iot_link_start_advertising(void *link, void *adv_data, size_t adv_data_len,uint32_t adv_time)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.start_advertising, 0);
    return iot_link->network.start_advertising(iot_link->handle, adv_data, adv_data_len,adv_time);
}

int qcloud_iot_link_stop_advertising(void *link)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.stop_advertising, 0);
    return iot_link->network.stop_advertising(iot_link->handle);
}

IotLinkProtocol qcloud_iot_link_get_protocol(void *link)
{
    POINTER_SANITY_CHECK(link, TCIOT_LINK_PROTOCOL_UNKOWN);
    IotLink *iot_link = link;
    return iot_link->network.protocol;
}

void qcloud_iot_link_destroy(void *link)
{
    POINTER_SANITY_CHECK_RTN(link);
    IotLink *iot_link = link;
    iot_link->network.deinit(iot_link->handle);
    TCI_HAL_Free(iot_link);
}

int qcloud_iot_link_device_describe_sync(void *link, void *cloud_dev_list)
{
    POINTER_SANITY_CHECK(link, 0);
    IotLink *iot_link = link;
    POINTER_SANITY_CHECK(iot_link->network.device_describe_sync, 0);
    return iot_link->network.device_describe_sync(iot_link->handle, cloud_dev_list, link);
}
