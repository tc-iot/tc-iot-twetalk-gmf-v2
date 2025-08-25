/*****************************************************************************
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the MIT License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef _TC_IOT_RET_CODE_H_
#define _TC_IOT_RET_CODE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IOT HAL return/error code
 * should be compatible with "tc_iot_ret_code.h"
 */
typedef enum {
    QCLOUD_RET_SUCCESS = 0,  // Successful return

    QCLOUD_ERR_FAILURE  = -1001,  // Generic failure return
    QCLOUD_ERR_INVAL    = -1002,  // Invalid parameter
    QCLOUD_ERR_DEV_INFO = -1003,  // Fail to get device info
    QCLOUD_ERR_MALLOC   = -1004,  // Fail to malloc memory

    QCLOUD_ERR_TCP_SOCKET_FAILED   = -601,  // TLS TCP socket connect fail
    QCLOUD_ERR_TCP_UNKNOWN_HOST    = -602,  // TCP unknown host (DNS fail)
    QCLOUD_ERR_TCP_CONNECT         = -603,  // TCP/UDP socket connect fail
    QCLOUD_ERR_TCP_READ_TIMEOUT    = -604,  // TCP read timeout
    QCLOUD_ERR_TCP_WRITE_TIMEOUT   = -605,  // TCP write timeout
    QCLOUD_ERR_TCP_READ_FAIL       = -606,  // TCP read error
    QCLOUD_ERR_TCP_WRITE_FAIL      = -607,  // TCP write error
    QCLOUD_ERR_TCP_PEER_SHUTDOWN   = -608,  // TCP server close connection
    QCLOUD_ERR_TCP_NOTHING_TO_READ = -609,  // TCP socket nothing to read

    QCLOUD_ERR_SSL_INIT            = -701,  // TLS/SSL init fail
    QCLOUD_ERR_SSL_CERT            = -702,  // TLS/SSL certificate issue
    QCLOUD_ERR_SSL_CONNECT         = -703,  // TLS/SSL connect fail
    QCLOUD_ERR_SSL_CONNECT_TIMEOUT = -704,  // TLS/SSL connect timeout
    QCLOUD_ERR_SSL_WRITE_TIMEOUT   = -705,  // TLS/SSL write timeout
    QCLOUD_ERR_SSL_WRITE           = -706,  // TLS/SSL write error
    QCLOUD_ERR_SSL_READ_TIMEOUT    = -707,  // TLS/SSL read timeout
    QCLOUD_ERR_SSL_READ            = -708,  // TLS/SSL read error
    QCLOUD_ERR_SSL_NOTHING_TO_READ = -709,  // TLS/SSL nothing to read

} IoTHalReturnCode;

#ifdef __cplusplus
}
#endif

#endif /* _TC_IOT_RET_CODE_H_ */
