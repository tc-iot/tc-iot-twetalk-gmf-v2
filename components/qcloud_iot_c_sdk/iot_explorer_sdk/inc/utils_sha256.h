/**
 * @file utils_sha256.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-12-28
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
 * 2022-12-28		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_SHA256_H_
#define IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_SHA256_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHA256_DIGEST_LENGTH        (32)
#define SHA256_BLOCK_LENGTH         (64)
#define SHA256_KEY_IOPAD_SIZE       (64)
#define SHA256_SHORT_BLOCK_LENGTH   (SHA256_BLOCK_LENGTH - 8)
#define SHA256_DIGEST_STRING_LENGTH (SHA256_DIGEST_LENGTH * 2 + 1)

/**
 * \brief          SHA-256 context structure
 */
typedef struct {
    uint32_t      total[2];   /*!< number of bytes processed  */
    uint32_t      state[8];   /*!< intermediate digest state  */
    unsigned char buffer[64]; /*!< data block being processed */
    int           is224;      /*!< 0 => SHA-256, else SHA-224 */
} IotSha256Context;

/**
 * @brief Initialize SHA-256 context.
 *
 * @param[in,out] ctx SHA-256 context to be initialized
 */
void utils_sha256_init(IotSha256Context *ctx);

/**
 * @brief Clear SHA-256 context.
 *
 * @param[in,out] ctx SHA-256 context to be cleared
 */
void utils_sha256_free(IotSha256Context *ctx);

/**
 * @brief SHA-256 context setup
 *
 * @param[in,out] ctx context to be initialized
 */
void utils_sha256_starts(IotSha256Context *ctx);

/**
 * @brief SHA-256 process
 *
 * @param[in,out] ctx pointer to ctx
 * @param[in] data data to be calculated
 */
void utils_sha256_process(IotSha256Context *ctx, const unsigned char data[64]);

/**
 * @brief SHA-256 process buffer.
 *
 * @param[in,out] ctx SHA-256 context
 * @param[in] input buffer holding the data
 * @param[in] ilen length of the input data
 */
void utils_sha256_update(IotSha256Context *ctx, const unsigned char *input, uint32_t ilen);

/**
 * @brief SHA-256 final digest
 *
 * @param[in,out] ctx SHA-256 context
 * @param[out] output SHA-256 checksum result
 */
void utils_sha256_finish(IotSha256Context *ctx, uint8_t output[32]);

/**
 * @brief Output = SHA-256( input buffer )
 *
 * @param[in] input buffer holding the data
 * @param[in] ilen length of the input data
 * @param[out] output SHA-256 checksum result
 */
void utils_sha256(const uint8_t *input, uint32_t ilen, uint8_t output[32]);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMMON_CRYPTOLOGY_INC_UTILS_SHA256_H_
