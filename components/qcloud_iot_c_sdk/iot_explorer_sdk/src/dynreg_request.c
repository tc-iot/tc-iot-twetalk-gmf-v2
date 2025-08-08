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
 * @file dynreg_request.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-10-12
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-10-12 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "qcloud_iot_dynreg.h"
#include "utils_base64.h"
#include "utils_hmac.h"

#define UTILS_AES_BLOCK_LEN (16)

/**
 * @brief send request
 *
 * @param response
 * @param response_len
 * @param product_id
 * @param device_name
 * @param product_secret
 * @param sign
 * @param time_stamp
 * @param nonce
 * @return > 0 for success
 */
static int _send_request(char *response, size_t response_len, const char *product_id, const char *device_name,
                         const char *product_secret, const char *sign, uint32_t time_stamp, int nonce)
{
    /* constructor dynreg http body */
    int request_body_len = HAL_Snprintf(response, DYN_RESPONSE_BUFF_LEN, "{\"ProductId\":\"%s\",\"DeviceName\":\"%s\"}",
                                        product_id, device_name);

    HttpSignedParams params = {
        .host            = DYN_REG_SERVER_URL,
        .uri             = DYN_REG_URI_PATH,
        .need_recv       = IOT_BOOL_TRUE,
        .recv_timeout_ms = 2000,
        .secret_key      = product_secret,
        .sign            = sign,
        .sign_len        = sign ? strlen(sign) : 0,
        .time_stamp      = time_stamp,
        .nonce           = nonce,
    };
    return IOT_HTTP_SignedRequest(&params, response, request_body_len, (uint8_t *)response, DYN_RESPONSE_BUFF_LEN);
}

/**
 * @brief parse device register response buffer
 *
 * @param response_buf
 * @param response_len
 * @return int 0 success other error
 */
static int _parse_response_result(char *response_buf, int response_len, DeviceInfo *device_info)
{
#define DECODE_BUFF_LEN (256)
    int            rc = 0;
    UtilsJsonValue value;
    size_t         olen;
    char           base64_decode_buf[DECODE_BUFF_LEN] = {0};
    // 1. find payload
    rc = utils_json_value_get("Response.Payload", strlen("Response.Payload"), response_buf, response_len, &value);
    if (rc) {
        Log_e("can not find payload. json : %s", response_buf);
        return rc;
    }
    // 2. base64 decode
    rc = utils_base64decode((uint8_t *)base64_decode_buf, DECODE_BUFF_LEN, &olen, value.value, value.value_len);
    if (rc) {
        Log_e("base64 decode error.");
        return QCLOUD_ERR_FAILURE;
    }
    return IOT_DynReg_ParseResult((uint8_t *)base64_decode_buf, (uint32_t)olen, device_info->product_secret,
                                  device_info->device_secret);
}

/**
 * @brief dynreg device, get device secret or device cert file and private key file from iot platform
 *
 * @param[in] params @see DeviceInfo
 * @return 0 is success other is failed
 */
int IOT_DynReg_Device(DeviceInfo *device_info)
{
    int rc = 0;
    POINTER_SANITY_CHECK(device_info, QCLOUD_ERR_INVAL);

    if (strlen(device_info->product_secret) < UTILS_AES_BLOCK_LEN) {
        Log_e("product key illegal");
        rc = QCLOUD_ERR_FAILURE;
        goto exit;
    }
    char     response[DYN_RESPONSE_BUFF_LEN] = {0};
    uint8_t  sign[SHA1_DIGEST_SIZE]          = {0};
    int      nonce                           = IOT_Timer_GetRandomNumber();
    uint32_t timestamp                       = 1609430400;  // default timestamp
    char     sign_base64[31]                 = {0};
    size_t   olen                            = 0;

    IOT_DynReg_CreatSign(sign, sizeof(sign), device_info, nonce, timestamp);
    rc = utils_base64encode(sign_base64, sizeof(sign_base64), &olen, sign, SHA1_DIGEST_SIZE);
    if (rc) {
        return rc;
    }
    rc = _send_request(response, DYN_RESPONSE_BUFF_LEN, device_info->product_id, device_info->device_name,
                       device_info->product_secret, sign_base64, timestamp, nonce);
    if (rc < 0) {
        goto exit;
    }
    rc = _parse_response_result(response, rc, device_info);
exit:
    return rc;
}

/**
 * @brief dynreg device as proxy
 *
 * @param device_info
 * @param time_stamp
 * @param nonce
 * @param sign
 * @param payload
 * @param payload_len
 * @return 0 for success
 */
int IOT_DynReg_DeviceProxy(DeviceInfo *device_info, uint32_t time_stamp, int nonce, const uint8_t *sign,
                           uint8_t *payload, size_t *payload_len)
{
    int rc = 0;
    POINTER_SANITY_CHECK(device_info, QCLOUD_ERR_INVAL);

    char   response[DYN_RESPONSE_BUFF_LEN] = {0};
    char   sign_base64[31]                 = {0};
    size_t olen                            = 0;

    rc = utils_base64encode(sign_base64, sizeof(sign_base64), &olen, sign, SHA1_DIGEST_SIZE);
    if (rc) {
        return rc;
    }

    rc = _send_request(response, DYN_RESPONSE_BUFF_LEN, device_info->product_id, device_info->device_name, NULL,
                       sign_base64, time_stamp, nonce);
    if (rc < 0) {
        return rc;
    }
    UtilsJsonValue value;
    //  find payload
    rc = utils_json_value_get("Response.Payload", strlen("Response.Payload"), response, rc, &value);
    if (rc) {
        Log_e("can not find payload. json : %s", response);
        return rc;
    }

    rc = utils_base64decode(payload, *payload_len, payload_len, value.value, value.value_len);
    if (rc) {
        Log_e("base64 decode error.");
        return rc;
    }

    return 0;
}
