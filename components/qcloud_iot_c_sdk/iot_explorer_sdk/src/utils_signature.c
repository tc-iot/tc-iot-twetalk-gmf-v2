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
 * @file utils_signature.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-09-27
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-09-27 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "utils_signature.h"
#include <inttypes.h>
#include <string.h>

/**
 * @brief Generate bind signature.
 * signature = HmacSha1(product_id|device_name;random;timestamp, local_psk)
 *
 * @param buf
 * @param buf_len
 * @param product_id
 * @param device_name
 * @param psk
 * @param random
 * @param timestamp
 * @param signature
 */
size_t utils_bind_sign_generate(uint8_t *buf, size_t buf_len, const char *product_id, const char *device_name,
                                const char *psk, uint32_t random, uint32_t timestamp, uint8_t *signature)
{
    /* 1. sign fmt : ${product_id}${device_name};${random};${expiration_time} */
    int sign_info_len =
        snprintf((char *)buf, buf_len, "%s%s;%" SCNu32 ";%" SCNu32, product_id, device_name, random, timestamp);
    /* 2. cal hmac sha1 */
    int rc = utils_hmac_sha1(buf, sign_info_len, (const uint8_t *)psk, strlen(psk), signature);
    return rc ? 0 : SHA1_DIGEST_SIZE;
}
