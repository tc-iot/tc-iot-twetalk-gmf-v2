/**
 * @file dynreg.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-01-26
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
 * 2022-01-26		1.0			hubertxxu		first commit
 * </table>
 */

#include "qcloud_iot_dynreg.h"
#include "utils_json.h"
#include "utils_base64.h"
#include "utils_hmac.h"
#include "utils_aes.h"

/**
 * @brief Parse dynamic registration result
 *
 * @param response response data
 * @param response_len response data length
 * @param product_secret
 * @param device_secret copy device secret into this.
 * @return 0 for success.
 */
int IOT_DynReg_ParseResult(uint8_t *response, size_t response_len, const char *product_secret, char *device_secret)
{
    int            rc = 0;
    UtilsJsonValue value;
    uint32_t       encryption_type = 0;

    // 2. aes cbc
    uint8_t iv[16] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};
    rc             = utils_aes_crypt_cbc(response, response_len, (uint8_t *)product_secret, 128, 1, iv, response);
    if (rc) {
        Log_e("aes cbc error. ");
        goto exit;
    }

    // 3. find psk
    rc = utils_json_get_uint32("encryptionType", strlen("encryptionType"), (const char *)response, response_len,
                               &encryption_type);
    if (rc) {
        Log_e("parse encryptionType error. json : %s", response);
        goto exit;
    }

    rc = utils_json_value_get("psk", strlen("psk"), (const char *)response, response_len, &value);
    if (rc) {
        Log_e("parse psk error. json : %s", response);
        goto exit;
    }

    // 4. copy psk to device info
    strncpy(device_secret, value.value, value.value_len);
    Log_d("device secret : %s", device_secret);
exit:
#undef DECODE_BUFF_LEN
    return rc;
}

/**
 * @brief Create dynamic registration http header sign
 *
 * @param sign_out_buf
 * @param sign_out_buf_len
 * @param device_info device info must include product secret
 * @param nonce
 * @param time_stamp
 * @return 0 for success
 */
int IOT_DynReg_CreatSign(uint8_t *sign_out_buf, int sign_out_buf_len, DeviceInfo *device_info, int nonce,
                         uint32_t time_stamp)
{
#define QCLOUD_SUPPORT_HMACSHA1 "hmacsha1"

    char request_buf_sha1[SHA1_DIGEST_SIZE * 2 + 1] = {0};
    char sign_string[256]                           = {0};
    int  request_body_len =
        HAL_Snprintf(sign_string, DYN_RESPONSE_BUFF_LEN, "{\"ProductId\":\"%s\",\"DeviceName\":\"%s\"}",
                     device_info->product_id, device_info->device_name);

    /* cal hmac sha1 */
    utils_sha1_hex((const uint8_t *)sign_string, request_body_len, (uint8_t *)request_buf_sha1);
    memset(sign_string, 0, sizeof(sign_string));
    /* create sign string */
    HAL_Snprintf(sign_string, sizeof(sign_string), "%s\n%s\n%s\n%s\n%s\n%d\n%d\n%s", "POST", DYN_REG_SERVER_URL,
                 DYN_REG_URI_PATH, "", QCLOUD_SUPPORT_HMACSHA1, time_stamp, nonce, request_buf_sha1);
    return utils_hmac_sha1((const uint8_t *)sign_string, strlen(sign_string), (uint8_t *)device_info->product_secret,
                           strlen(device_info->product_secret), sign_out_buf)
               ? 0
               : SHA1_DIGEST_SIZE;
}
