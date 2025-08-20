/**
 * @file ble_qiot_llsync_device.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-11-25
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
 * 2022-11-25		1.0			hubertxxu		first commit
 * </table>
 */

#include "qcloud_iot_llsync_config.h"
#include "qcloud_iot_platform.h"
#include "utils_hmac.h"
#include "utils_base64.h"
#include "utils_aes.h"
#include "utils_md5.h"

#include "ble_llsync_device_info.h"
#include "ble_llsync_service.h"

#define BLE_GET_EXPIRATION_TIME(_cur_time) ((_cur_time) + BLE_EXPIRATION_TIME)
#define MD5_DIGEST_SIZE                    (16)

typedef struct {
    BLELLsyncCoreData     core_data;
    DeviceInfo           *dev_info;
    BLEConnectState       ble_connect_state;
    BLELLsyncConnectState llsync_connect_state;
    uint16_t              llsync_mtu;
    uint8_t               mac[6];
} BLELLsyncData;

static BLELLsyncData sg_llsync_data;

/**
 * @brief display bind state
 *
 * @param state @see BLELLsyncBindState
 */
static void _display_bind_state(BLELLsyncBindState state)
{
    const char *bind_state[] = {
        [E_LLSYNC_BIND_IDLE] = "unbind.",
        [E_LLSYNC_BIND_WAIT] = "binding...",
        [E_LLSYNC_BIND_SUCC] = "bound.",
    };
    Log_w("current bind state >>> %s", bind_state[state]);
}

/**
 * @brief init llsync data
 *
 * @param[in] mac mac addr
 * @return 0 for success
 */
int llsync_data_init(uint8_t mac[6], DeviceInfo *dev_info)
{
    int rc = 0;
    memset(&sg_llsync_data, 0, sizeof(BLELLsyncData));
#if BLE_QIOT_LLSYNC_STANDARD
    // 1. get core data
    rc = HAL_File_Read(BLE_LLSYNC_CORE_DATA_FILEPATH, &sg_llsync_data.core_data, sizeof(BLELLsyncCoreData), 0);
    if (!rc) {
        Log_w("The device has not been bound.");
    }
#endif // BLE_QIOT_LLSYNC_STANDARD
    // 2. get device info
    sg_llsync_data.dev_info = dev_info;
    // 3. get mac
    memcpy(sg_llsync_data.mac, mac, 6);
    // 4. display bind state
    _display_bind_state(sg_llsync_data.core_data.bind_state);
    // 5. set default mtu
    sg_llsync_data.llsync_mtu = ATT_MTU_TO_LLSYNC_MTU(ATT_DEFAULT_MTU);
    return 0;
}

/**
 * @brief creat llsync adv data
 *
 * @param[out] adv_data_raw adv data raw data buffer. length >= 32
 * @return adv data length
 */
int iot_llsync_creat_adv_data(uint8_t adv_data_raw[32])
{
#define SAMPLE_DEVICE_NAME "l"  // ble adv name, only one char.
    int     index               = 0;
    int     len                 = 0;
    uint8_t llsync_adv_data[20] = {0};

    static const uint8_t adv_data_fixed[7] = {
        /* flags */
        0x02,
        0x01,
        0x06,
        /* service uuid */
        0x03,
        0x03,
        (IOT_BLE_UUID_SERVICE & 0xff),
        (IOT_BLE_UUID_SERVICE >> 8) & 0xff,
    };

    // 1. creat llsync adv data
    llsync_adv_data[0] = TENCENT_COMPANY_IDENTIFIER & 0xff;
    llsync_adv_data[1] = (TENCENT_COMPANY_IDENTIFIER >> 8) & 0xff;
    index              = 2;

    llsync_adv_data[index] =
        sg_llsync_data.core_data.bind_state | (BLE_QIOT_LLSYNC_PROTOCOL_VERSION << LLSYNC_PROTO_VER_BIT);

#if BLE_QIOT_DYNREG_ENABLE
    if (is_llsync_need_dynreg()) {
        llsync_adv_data[index] |= (1 << LLSYNC_DYNREG_MASK_BIT);
    }
#endif
    index++;

    switch (sg_llsync_data.core_data.bind_state) {
        case E_LLSYNC_BIND_SUCC: {
            uint8_t md5_in_buf[128] = {0};
            uint8_t md5_in_len      = 0;
            // 1 bytes state + 8 bytes device identify_str + 8 bytes identify_str
            memcpy((char *)md5_in_buf, sg_llsync_data.dev_info->product_id, MAX_SIZE_OF_PRODUCT_ID);
            md5_in_len += MAX_SIZE_OF_PRODUCT_ID;
            memcpy((char *)md5_in_buf + md5_in_len, sg_llsync_data.dev_info->device_name,
                   strlen(sg_llsync_data.dev_info->device_name));
            md5_in_len += strlen(sg_llsync_data.dev_info->device_name);
            IotMd5Context ctx;
            utils_md5_reset(&ctx);
            utils_md5_update(&ctx, md5_in_buf, md5_in_len);
            utils_md5_finish(&ctx);
            for (int i = 0; i < MD5_DIGEST_SIZE / 2; i++) {
                llsync_adv_data[i + index] = ctx.md5sum_str[i] ^ ctx.md5sum_str[i + MD5_DIGEST_SIZE / 2];
            }
            index += MD5_DIGEST_SIZE / 2;
            memcpy(llsync_adv_data + index, sg_llsync_data.core_data.identify_str,
                   sizeof(sg_llsync_data.core_data.identify_str));
            index += sizeof(sg_llsync_data.core_data.identify_str);
        } break;
        // unbind
        case E_LLSYNC_BIND_IDLE:
        case E_LLSYNC_BIND_WAIT: {
#if BLE_QIOT_LLSYNC_CONFIG_NET && !BLE_QIOT_LLSYNC_DUAL_COM
            // llsync_adv_data[index++] = BLE_QIOT_LLSYNC_PROTOCOL_VERSION;
#endif  // BLE_QIOT_LLSYNC_CONFIG_NET
        // 1 bytes state + 6 bytes mac + 10 bytes product id
            memcpy(llsync_adv_data + index, sg_llsync_data.mac, 6);
            index += 6;
            memcpy(llsync_adv_data + index, sg_llsync_data.dev_info->product_id, MAX_SIZE_OF_PRODUCT_ID);
            index += MAX_SIZE_OF_PRODUCT_ID;
        } break;
        default:
            Log_w("bind state : %d error.", sg_llsync_data.core_data.bind_state);
            break;
    }

    // 2. creat adv data
    len = index;
    memcpy(adv_data_raw, adv_data_fixed, 7);
    index                 = 7;
    adv_data_raw[index++] = len + 1;
    adv_data_raw[index++] = 0xFF;
    memcpy(adv_data_raw + index, llsync_adv_data, len);
    index += len;
    // adv name
    adv_data_raw[index++] = 0x02;
    adv_data_raw[index++] = 0x09;
    memcpy(adv_data_raw + index, SAMPLE_DEVICE_NAME, 1);
    index += 1;
    return index;
}

/**
 * @brief get mtu
 *
 * @return mtu length
 */
uint16_t llsync_mtu_get(void)
{
    return sg_llsync_data.llsync_mtu;
}

/**
 * @brief update mtu
 *
 * @param[in] llsync_mtu new llsync mtu
 */
void llsync_mtu_update(uint16_t llsync_mtu)
{
    Log_i("update MTU %d -> %d", sg_llsync_data.llsync_mtu, llsync_mtu);
    sg_llsync_data.llsync_mtu = llsync_mtu;
}

/**
 * @brief set llsync bind state
 *
 * @param[in] new_state
 */
void llsync_bind_state_set(BLELLsyncBindState new_state)
{
    Log_d("bind state: %d >>> %d", sg_llsync_data.core_data.bind_state, new_state);
    sg_llsync_data.core_data.bind_state = new_state;
}

/**
 * @brief get llsync bind state
 *
 * @return @see BLELLsyncBindState
 */
BLELLsyncBindState llsync_bind_state_get(void)
{
    return sg_llsync_data.core_data.bind_state;
}

/**
 * @brief set llsync connect state
 *
 * @param new_state
 */
void llsync_connection_state_set(BLELLsyncConnectState new_state)
{
    Log_d("llsync connect state: %d ---> %d", sg_llsync_data.llsync_connect_state, new_state);
    sg_llsync_data.llsync_connect_state = new_state;
}

/**
 * @brief check llsync connect state
 *
 * @return IotBool
 */
IotBool llsync_is_connected(void)
{
    if (sg_llsync_data.llsync_connect_state == E_LLSYNC_CONNECTED) {
        return IOT_BOOL_TRUE;
    }
    return IOT_BOOL_FALSE;
}

/**
 * @brief set ble connect state
 *
 * @param new_state
 */
void ble_connection_state_set(BLEConnectState new_state)
{
    Log_d("ble connect state: %d ---> %d", sg_llsync_data.ble_connect_state, new_state);
    sg_llsync_data.ble_connect_state = new_state;
}

/**
 * @brief check ble connect state
 *
 * @return IotBool
 */
IotBool ble_is_connected(void)
{
    if (sg_llsync_data.ble_connect_state == E_BLE_CONNECTED) {
        return IOT_BOOL_TRUE;
    }
    return IOT_BOOL_FALSE;
}

/**
 * @brief update mtu
 *
 * @param[in] result wechat mini app result buffer
 * @param[in] data_len result buffer length
 * @return 0 for success
 */
int ble_inform_mtu_result(const char *result, uint16_t data_len)
{
    uint16_t rc = 0;

    memcpy(&rc, result, sizeof(uint16_t));
    rc = NTOHS(rc);
    Log_i("mtu setting result: %02x", rc);

    if (LLSYNC_MTU_SET_RESULT_ERR == rc) {
        return ble_uplink_sync_mtu(ATT_DEFAULT_MTU);
    } else if (0 == rc) {
        extern int qcloud_iot_link_ble_get_mtu(void *handle);
        uint16_t   mtu = qcloud_iot_link_ble_get_mtu(NULL);
        Log_i("current mtu : %d", mtu);
        return ble_uplink_sync_mtu(ATT_USER_MTU);
    } else {
        llsync_mtu_update(ATT_MTU_TO_LLSYNC_MTU(rc));
        return QCLOUD_RET_SUCCESS;
    }
}

#if BLE_QIOT_LLSYNC_STANDARD

#if BLE_QIOT_DYNREG_ENABLE

#define UTILS_AES_ENCRYPT   1 /**< AES encryption. */
#define UTILS_AES_DECRYPT   0 /**< AES decryption. */
#define UTILS_AES_BLOCK_LEN 16
#define AES_KEY_BITS_128    128

/**
 * @brief check llsync need dynamic register or not
 *
 * @return IotBool
 */
IotBool is_llsync_need_dynreg(void)
{
    if (!sg_llsync_data.dev_info->device_secret[0] || !strncmp(sg_llsync_data.dev_info->device_secret, "IOT_PSK", 7)) {
        Log_d("need dyn register.");
        return IOT_BOOL_TRUE;
    }
    return IOT_BOOL_FALSE;
}

/**
 * @brief get dynamic register auth code
 *
 * @param[in] bind_data wechat mini app bind data buffer
 * @param[in] data_len  bind data buffer length
 * @param[out] out_buf  bind out buffer
 * @param[out] buf_len  bind out buffer length
 * @return real out buffer length
 */
int ble_dynreg_get_authcode(const char *bind_data, uint16_t data_len, char *out_buf, uint16_t buf_len)
{
    POINTER_SANITY_CHECK(bind_data, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(out_buf, QCLOUD_ERR_INVAL);

    const char *sign_fmt         = "deviceName=%s&nonce=%u&productId=%.10s&timestamp=%u";
    uint32_t    timestamp        = 0;
    uint32_t    nonce            = 0;
    char        sign_source[128] = {0};
    uint8_t     sign_len         = 0;
    int         ret_len          = 0;

    BLELLsyncBindData bind_data_aligned;
    memcpy(&bind_data_aligned, bind_data, sizeof(BLELLsyncBindData));
    nonce     = NTOHL(bind_data_aligned.nonce);
    timestamp = NTOHL(bind_data_aligned.timestamp);

    sign_len = snprintf(sign_source, sizeof(sign_source), sign_fmt, sg_llsync_data.dev_info->device_name, nonce,
                        sg_llsync_data.dev_info->product_id, timestamp);
    utils_hmac_sha1_hex((uint8_t *)sign_source, sign_len, (uint8_t *)sg_llsync_data.dev_info->product_secret,
                        sizeof(sg_llsync_data.dev_info->product_secret), sign_source);

    out_buf[ret_len++] = strlen(sg_llsync_data.dev_info->device_name);
    memcpy(out_buf + ret_len, sg_llsync_data.dev_info->device_name, out_buf[0]);
    ret_len += out_buf[0];
    /*base64 encode*/
    utils_base64encode(out_buf + ret_len, buf_len - ret_len, (size_t *)&sign_len, (const uint8_t *)sign_source,
                       SHA1_DIGEST_SIZE * 2);
    ret_len += sign_len;
    return ret_len;
}

/**
 * @brief pase dynamic register result
 *
 * @param[in] in_buf wechat mini app dynamic register result buffer
 * @param[in] data_len buffer length
 * @return 0 for success
 */
int ble_dynreg_parse_psk(const char *in_buf, uint16_t data_len)
{
    int           rc              = 0;
    char          decodeBuff[128] = {0};
    size_t        len;
    int           datalen;
    unsigned int  keybits;
    char          key[UTILS_AES_BLOCK_LEN + 1];
    unsigned char iv[16];
    char         *psk = NULL;

    rc = utils_base64decode((uint8_t *)decodeBuff, sizeof(decodeBuff), &len, in_buf, data_len);
    if (rc != 0) {
        Log_e("Response decode err, response:%s", in_buf);
        return QCLOUD_ERR_FAILURE;
    }

    datalen = len + (UTILS_AES_BLOCK_LEN - len % UTILS_AES_BLOCK_LEN);
    keybits = AES_KEY_BITS_128;
    memset(key, 0, UTILS_AES_BLOCK_LEN);
    strncpy(key, sg_llsync_data.dev_info->product_secret, UTILS_AES_BLOCK_LEN);
    memset(iv, '0', UTILS_AES_BLOCK_LEN);
    rc = utils_aes_crypt_cbc((uint8_t *)decodeBuff, datalen, (uint8_t *)key, keybits, UTILS_AES_DECRYPT, iv,
                             (uint8_t *)decodeBuff);
    if (!rc) {
        Log_i("The decrypted data is:%s", decodeBuff);
    } else {
        Log_e("data decry err, rc:%d", rc);
        return QCLOUD_ERR_FAILURE;
    }

    psk = strstr(decodeBuff, "\"psk\"");
    if (NULL != psk) {
        memcpy(sg_llsync_data.dev_info->device_secret, psk + strlen("\"psk\":\""), MAX_SIZE_OF_DEVICE_SECRET);
        Log_d("device secret : %s", sg_llsync_data.dev_info->device_secret);
        HAL_SetDevInfo(&sg_llsync_data.dev_info);
        return QCLOUD_RET_SUCCESS;
    }
    Log_e("no-exist psk");
    return QCLOUD_ERR_FAILURE;
}
#endif

/**
 * @brief storage ble llsync core data
 *
 * @param[in] core_data @see BLELLsyncCoreData
 * @return 0 for success
 */
static int ble_write_core_data(BLELLsyncCoreData *core_data)
{
    memcpy(&sg_llsync_data.core_data, core_data, sizeof(BLELLsyncCoreData));
    Log_dump("core data", (void *)&sg_llsync_data.core_data, sizeof(sg_llsync_data.core_data));
    if (sizeof(BLELLsyncCoreData) != HAL_File_Write(BLE_LLSYNC_CORE_DATA_FILEPATH, (char *)&sg_llsync_data.core_data,
                                                    sizeof(BLELLsyncCoreData), 0)) {
        Log_e("llsync write core failed");
        return QCLOUD_ERR_FAILURE;
    }
    return QCLOUD_RET_SUCCESS;
}

int iot_llsync_set_core_data(uint8_t bind_state, uint32_t local_psk, uint8_t identify_str[8])
{
    BLELLsyncCoreData core_data;
    core_data.bind_state = bind_state;
    memcpy(core_data.local_psk, &local_psk, BLE_LOCAL_PSK_LEN);
    memcpy(core_data.identify_str, identify_str, BLE_BIND_IDENTIFY_STR_LEN);
    return ble_write_core_data(&core_data);
}

/**
 * @brief get bind auth code
 *
 * @param[in] bind_data wechat mini app bind auth code data buffer
 * @param[in] data_len wechat mini app bind auth code data buffer length
 * @param[out] out_buf auth code output buffer
 * @param[out] buf_len auth code output buffer length
 * @return real output buffer length
 */
int ble_bind_get_authcode(const char *bind_data, uint16_t data_len, char *out_buf, uint16_t buf_len)
{
    POINTER_SANITY_CHECK(bind_data, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(data_len, (uint16_t)sizeof(BLELLsyncBindData), QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(out_buf, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(buf_len, SHA1_DIGEST_SIZE + MAX_SIZE_OF_DEVICE_NAME, QCLOUD_ERR_INVAL);

    char    out_sign[SHA1_DIGEST_SIZE]                = {0};
    char    sign_info[80]                             = {0};
    int     sign_info_len                             = 0;
    int     time_expiration                           = 0;
    int     nonce                                     = 0;
    int     ret_len                                   = 0;
    uint8_t secret[MAX_SIZE_OF_DEVICE_SECRET / 4 * 3] = {0};
    int     secret_len                                = 0;

    // if the pointer "char *bind_data" is not aligned with 4 byte, in some cpu convert it to
    // pointer "ble_bind_data *" work correctly, but some cpu will get wrong value, or cause
    // other "Unexpected Error". Here copy it to a local variable make sure aligned with 4 byte.
    BLELLsyncBindData bind_data_aligned;
    memcpy(&bind_data_aligned, bind_data, sizeof(BLELLsyncBindData));

    nonce           = NTOHL(bind_data_aligned.nonce);
    time_expiration = BLE_GET_EXPIRATION_TIME(NTOHL(bind_data_aligned.timestamp));
    // [10 bytes product_id] + [x bytes device name] + ';' + [4 bytes nonce] + ';' + [4 bytes timestamp]
    memcpy(sign_info, sg_llsync_data.dev_info->product_id, MAX_SIZE_OF_PRODUCT_ID);
    sign_info_len += MAX_SIZE_OF_PRODUCT_ID;
    memcpy(sign_info + sign_info_len, sg_llsync_data.dev_info->device_name,
           strlen(sg_llsync_data.dev_info->device_name));
    sign_info_len += strlen(sg_llsync_data.dev_info->device_name);
    snprintf(sign_info + sign_info_len, sizeof(sign_info) - sign_info_len, ";%u", nonce);
    sign_info_len += strlen(sign_info + sign_info_len);
    snprintf(sign_info + sign_info_len, sizeof(sign_info) - sign_info_len, ";%u", time_expiration);
    sign_info_len += strlen(sign_info + sign_info_len);
    Log_dump("bind sign in", sign_info, sign_info_len);

    utils_base64decode(secret, sizeof(secret), (size_t *)&secret_len,
                       (const char *)sg_llsync_data.dev_info->device_secret,
                       strlen(sg_llsync_data.dev_info->device_secret));

    utils_hmac_sha1((uint8_t *)sign_info, sign_info_len, secret, secret_len, (uint8_t *)out_sign);

    Log_dump("bind sign out", out_sign, sizeof(out_sign));

    // return [20 bytes sign] + [x bytes device_name]
    memset(out_buf, 0, buf_len);
    memcpy(out_buf, out_sign, SHA1_DIGEST_SIZE);
    ret_len = SHA1_DIGEST_SIZE;

    memcpy(out_buf + ret_len, sg_llsync_data.dev_info->device_name, strlen(sg_llsync_data.dev_info->device_name));
    ret_len += strlen(sg_llsync_data.dev_info->device_name);
    Log_dump("bind auth code", out_buf, ret_len);

    return ret_len;
}

/**
 * @brief set llsync bind result
 *
 * @param[in] result wechat mini app result buffer
 * @param[in] len wechat mini app result buffer length
 * @return 0 for success
 */
int ble_bind_write_result(const char *result, uint16_t len)
{
    POINTER_SANITY_CHECK(result, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(len, (uint16_t)sizeof(BLELLsyncCoreData), QCLOUD_ERR_INVAL);

    BLELLsyncCoreData *bind_result = (BLELLsyncCoreData *)result;

    llsync_bind_state_set(bind_result->bind_state);
    return ble_write_core_data(bind_result);
}

/**
 * @brief clear bind state
 *
 * @return 0 for success
 */
int ble_unbind_write_result(void)
{
    BLELLsyncCoreData bind_result;
    llsync_connection_state_set(E_LLSYNC_DISCONNECTED);
    llsync_bind_state_set(E_LLSYNC_BIND_IDLE);
    memset(&bind_result, 0, sizeof(bind_result));
    return ble_write_core_data(&bind_result);
}

/**
 * @brief get connect auth code
 *
 * @param[in] conn_data wechat mini app connect data buffer
 * @param[in] data_len  wechat mini app connect data buffer length
 * @param[out] out_buf  connect buffer output data buffer
 * @param[out] buf_len  connect buffer output data buffer length
 * @return real connect buffer output data length
 */
int ble_conn_get_authcode(const char *conn_data, uint16_t data_len, char *out_buf, uint16_t buf_len)
{
    POINTER_SANITY_CHECK(conn_data, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(data_len, (uint16_t)sizeof(BLELLsyncConnectData), QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(out_buf, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(buf_len, SHA1_DIGEST_SIZE + MAX_SIZE_OF_DEVICE_NAME, QCLOUD_ERR_INVAL);

    char sign_info[64]              = {0};
    char out_sign[SHA1_DIGEST_SIZE] = {0};
    int  sign_info_len              = 0;
    int  timestamp                  = 0;
    int  ret_len                    = 0;

    // if the pointer "char *bind_data" is not aligned with 4 byte, in some cpu convert it to
    // pointer "ble_bind_data *" work correctly, but some cpu will get wrong value, or cause
    // other "Unexpected Error". Here copy it to a local variable make sure aligned with 4 byte.
    BLELLsyncConnectData conn_data_aligned;
    memcpy(&conn_data_aligned, conn_data, sizeof(BLELLsyncConnectData));

    // 1.valid sign
    timestamp = NTOHL(conn_data_aligned.timestamp);
    snprintf(sign_info + sign_info_len, sizeof(sign_info) - sign_info_len, "%d", timestamp);
    sign_info_len = strlen(sign_info + sign_info_len);
    Log_dump("valid sign in", sign_info, sign_info_len);
    utils_hmac_sha1((uint8_t *)sign_info, sign_info_len, (uint8_t *)sg_llsync_data.core_data.local_psk,
                    sizeof(sg_llsync_data.core_data.local_psk), (uint8_t *)out_sign);
    Log_dump("valid sign out", out_sign, SHA1_DIGEST_SIZE);
    if (0 != memcmp(&conn_data_aligned.sign_info, out_sign, SHA1_DIGEST_SIZE)) {
        Log_e("llsync invalid connect sign");
        return QCLOUD_ERR_INVAL;
    }

    // 2.calc sign
    memset(sign_info, 0, sizeof(sign_info));
    sign_info_len = 0;

    // expiration time + product id + device name
    timestamp = BLE_GET_EXPIRATION_TIME(NTOHL(conn_data_aligned.timestamp));
    snprintf(sign_info + sign_info_len, sizeof(sign_info) - sign_info_len, "%d", timestamp);
    sign_info_len += strlen(sign_info + sign_info_len);
    memcpy(sign_info + sign_info_len, sg_llsync_data.dev_info->product_id, MAX_SIZE_OF_PRODUCT_ID);
    sign_info_len += MAX_SIZE_OF_PRODUCT_ID;
    memcpy(sign_info + sign_info_len, sg_llsync_data.dev_info->device_name,
           strlen(sg_llsync_data.dev_info->device_name));
    sign_info_len += strlen(sg_llsync_data.dev_info->device_name);

    Log_dump("conn sign in", sign_info, sign_info_len);
    utils_hmac_sha1((uint8_t *)sign_info, sign_info_len, (uint8_t *)sg_llsync_data.core_data.local_psk,
                    sizeof(sg_llsync_data.core_data.local_psk), (uint8_t *)out_sign);
    Log_dump("conn sign out", out_sign, sizeof(out_sign));

    // 3.return auth code
    memset(out_buf, 0, buf_len);
    memcpy(out_buf, out_sign, SHA1_DIGEST_SIZE);
    ret_len += SHA1_DIGEST_SIZE;
    memcpy(out_buf + ret_len, sg_llsync_data.dev_info->device_name, strlen(sg_llsync_data.dev_info->device_name));
    ret_len += strlen(sg_llsync_data.dev_info->device_name);
    Log_dump("conn auth code", out_buf, ret_len);

    return ret_len;
}

/**
 * @brief get unbind auth code
 *
 * @param[in] conn_data wechat mini app unbind data buffer
 * @param[in] data_len  wechat mini app unbind data buffer length
 * @param[out] out_buf  unbind buffer output data buffer
 * @param[out] buf_len  unbind buffer output data buffer length
 * @return real unbind buffer output data length
 */
int ble_unbind_get_authcode(const char *unbind_data, uint16_t data_len, char *out_buf, uint16_t buf_len)
{
    POINTER_SANITY_CHECK(unbind_data, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(data_len, (uint16_t)sizeof(BLELLsyncUnbindData), QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(out_buf, QCLOUD_ERR_INVAL);
    BUFF_LEN_SANITY_CHECK(buf_len, SHA1_DIGEST_SIZE, QCLOUD_ERR_INVAL);

    char sign_info[32]              = {0};
    char out_sign[SHA1_DIGEST_SIZE] = {0};
    int  sign_info_len              = 0;
    int  ret_len                    = 0;

    // 1.valid sign
    memcpy(sign_info, BLE_UNBIND_REQUEST_STR, BLE_UNBIND_REQUEST_STR_LEN);
    sign_info_len = BLE_UNBIND_REQUEST_STR_LEN;
    utils_hmac_sha1((uint8_t *)sign_info, sign_info_len, (uint8_t *)sg_llsync_data.core_data.local_psk,
                    sizeof(sg_llsync_data.core_data.local_psk), (uint8_t *)out_sign);
    Log_dump("valid sign out", out_sign, SHA1_DIGEST_SIZE);

    if (0 != memcmp(((BLELLsyncUnbindData *)unbind_data)->sign_info, out_sign, SHA1_DIGEST_SIZE)) {
        Log_e("llsync invalid unbind sign");
        return QCLOUD_ERR_INVAL;
    }

    // 2.calc sign
    memset(sign_info, 0, sizeof(sign_info));
    sign_info_len = 0;

    memcpy(sign_info, BLE_UNBIND_RESPONSE, strlen(BLE_UNBIND_RESPONSE));
    sign_info_len += BLE_UNBIND_RESPONSE_STR_LEN;
    utils_hmac_sha1((uint8_t *)sign_info, sign_info_len, (uint8_t *)sg_llsync_data.core_data.local_psk,
                    sizeof(sg_llsync_data.core_data.local_psk), (uint8_t *)out_sign);
    Log_dump("unbind auth code", out_sign, SHA1_DIGEST_SIZE);

    // 3.return auth code
    memset(out_buf, 0, buf_len);
    memcpy(out_buf, out_sign, sizeof(out_sign));
    ret_len += sizeof(out_sign);

    return ret_len;
}
#endif  // BLE_QIOT_LLSYNC_STANDARD

/**
 * @brief get device name
 *
 * @param[out] output_name device name buffer
 * @return real device name length
 */
int ble_get_device_name(char *output_name)
{
    if (sg_llsync_data.dev_info->device_name[0] && strncmp(sg_llsync_data.dev_info->device_secret, "IOT_PSK", 7)) {
        strncpy(output_name, sg_llsync_data.dev_info->device_name, MAX_SIZE_OF_DEVICE_NAME);
        return (int)strlen(sg_llsync_data.dev_info->device_name);
    }
    return QCLOUD_ERR_FAILURE;
}

int ble_get_device_version(char *output_version)
{
    strncpy(output_version, sg_llsync_data.dev_info->device_version, MAX_SIZE_OF_DEVICE_VERSION_LENGTH);
    return (int)strlen(sg_llsync_data.dev_info->device_version);
}


