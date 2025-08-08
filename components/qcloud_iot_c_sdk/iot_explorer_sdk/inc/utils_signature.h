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
 * @file utils_signature.h
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

#ifndef IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_SIGNATURE_H_
#define IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_SIGNATURE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "utils_hmac.h"
#include "utils_base64.h"

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
                                const char *psk, uint32_t random, uint32_t timestamp, uint8_t *signature);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_SIGNATURE_H_
