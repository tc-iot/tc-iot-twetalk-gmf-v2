/**
 * @file utils_crc.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief 
 * @version 1.0
 * @date 2023-05-17
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
 * 2023-05-17		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_CRC_H_
#define IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_CRC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

/**
 * @brief 
 * 
 * @param crc last crc value
 * @param buf crc buffer
 * @param len crc buffer length
 * @return uint32_t crc32 result
 */
uint32_t utils_crc32(uint32_t crc, const uint8_t *buf, int len);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_CRC_H_
