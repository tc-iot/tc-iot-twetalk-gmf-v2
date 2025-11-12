/**
 * @file ble_qiot_service.c
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
#include "qcloud_iot_llsync.h"
#include "ble_llsync_device_info.h"
#include "ble_llsync_service.h"
#if BLE_QTCIOT_LLSYNC_GATEWAY
#include "ble_llsync_to_lltlv.h"
#endif // BLE_QTCIOT_LLSYNC_GATEWAY
#include "utils_crc.h"

#if BLE_QTCIOT_SUPPORT_OTA

typedef struct {
    uint32_t file_size;
    uint32_t file_crc;
    uint8_t  file_version[BLE_QTCIOT_OTA_MAX_VERSION_STR];
} BleOtaFileInfo;

// ota info saved in flash if support resuming
typedef struct {
    uint8_t        valid_flag;
    uint8_t        rsv[3];
    uint32_t       last_file_size;  // the file size already write in flash
    uint32_t       last_address;    // the address file saved
    BleOtaFileInfo download_file_info;
} BleOtaInfoRecord;

typedef struct {
    uint32_t file_size;
    uint8_t  req;
} BleOtaReply;

// ota request reply
typedef struct {
    uint8_t  package_nums;      // package numbers in a loop
    uint8_t  package_size;      // each package size
    uint8_t  retry_timeout;     // data retry
    uint8_t  reboot_timeout;    // max time of device reboot
    uint32_t last_file_size;    // the file already received
    uint8_t  package_interval;  // package send interval on the server
    uint8_t  rsv[3];
} BleOtaReplyInfo;

typedef struct {
    void            *ota_timer;
    uint8_t          timeout_cnt;
    uint8_t          data_buf[BLE_QTCIOT_OTA_BUF_SIZE];
    uint16_t         data_buf_size;
    uint32_t         download_file_size;
    uint8_t          next_seq;
    uint32_t         download_address;
    uint8_t          download_percent;
    uint8_t          ota_flag;
    BleOtaReply      ota_reply_info;
    BleOtaInfoRecord ota_info_record;
} BleOta;
#endif  // BLE_QTCIOT_SUPPORT_OTA

typedef struct {
    IotBool                  init_flag;
    void                    *link;
    TCIOTBLELLsyncCallback     callback;
    void                    *usr_data;
    TCIOTBLELLsyncWifiCallback wifi_callback;
    void                    *wifi_usr_data;
#if BLE_QTCIOT_SUPPORT_OTA
    BleOta ota;
#endif
} TCIOTBLELLsync;

static TCIOTBLELLsync sg_iot_ble_llsync = {
    .init_flag = TCIOT_BOOL_FALSE,
};

// -----------------------------------------------------------------------------
// uplink data handle
// -----------------------------------------------------------------------------
/**
 * @brief send data to wechat mini app
 *
 * @param uplink_data @see LLsyncUpLinkData
 * @return 0 for success
 */
int ble_uplink_notify2(LLsyncUpLinkData *uplink_data)
{
    char              *p              = (char *)uplink_data->buf;
    uint16_t           left_len       = uplink_data->buf_len;
    uint16_t           send_len       = 0;
    uint16_t           mtu_size       = 0;
    BLELLsyncSliceType slice_state    = BLE_QTCIOT_EVENT_NO_SLICE;
    uint16_t           send_buf_index = 0;
    uint16_t           tmp_len        = 0;

    uint8_t send_buf[BLE_QTCIOT_EVENT_BUF_SIZE] = {0};

    if (!llsync_is_connected() && uplink_data->type != BLE_QTCIOT_EVENT_UP_BIND_SIGN_RET &&
        uplink_data->type != BLE_QTCIOT_EVENT_UP_CONN_SIGN_RET &&
        uplink_data->type != BLE_QTCIOT_EVENT_UP_UNBIND_SIGN_RET &&
        uplink_data->type != BLE_QTCIOT_EVENT_UP_SYNC_WAIT_TIME && uplink_data->type != BLE_QTCIOT_EVENT_UP_DYNREG_SIGN) {
        Log_e("upload msg negate, device not connected, uplink_data->type : %d", uplink_data->type);
        return QCLOUD_ERR_FAILURE;
    }

    // reserve event header length, 3 bytes fixed length + n bytes header
    mtu_size = llsync_mtu_get();
    mtu_size = mtu_size > sizeof(send_buf) ? sizeof(send_buf) : mtu_size;
    mtu_size -= (BLE_QTCIOT_EVENT_FIXED_HEADER_LEN + uplink_data->header_len);
    // Log_d("mtu size %d", mtu_size);

    do {
        memset(send_buf, 0, sizeof(send_buf));
        send_buf_index = 0;
        send_len       = left_len > mtu_size ? mtu_size : left_len;

        send_buf[send_buf_index++] = uplink_data->type;
        if (NULL != uplink_data->buf) {
            //tmp_len = HTONS(send_len + uplink_data->header_len);
            {
                // TODO: 适配macos
                uint16_t temp = send_len + uplink_data->header_len;
                tmp_len = HTONS(temp);
            }
            memcpy(send_buf + send_buf_index, &tmp_len, sizeof(uint16_t));
            send_buf_index += sizeof(uint16_t);
            if (NULL != uplink_data->header) {
                memcpy(send_buf + send_buf_index, uplink_data->header, uplink_data->header_len);
                send_buf_index += uplink_data->header_len;
            }
            memcpy(send_buf + send_buf_index, p, send_len);
            send_buf_index += send_len;

            p += send_len;
            left_len -= send_len;
            send_len += (BLE_QTCIOT_EVENT_FIXED_HEADER_LEN + uplink_data->header_len);

            if (0 == left_len) {
                slice_state =
                    (BLE_QTCIOT_EVENT_NO_SLICE == slice_state) ? BLE_QTCIOT_EVENT_NO_SLICE : BLE_QTCIOT_EVENT_SLICE_FOOT;
            } else {
                slice_state =
                    (BLE_QTCIOT_EVENT_NO_SLICE == slice_state) ? BLE_QTCIOT_EVENT_SLICE_HEAD : BLE_QTCIOT_EVENT_SLICE_BODY;
            }
            // the high 2 bits means slice state, and the left 14 bits is data length
            send_buf[1] |= slice_state << 6;
            send_buf[1] |= uplink_data->length_flag;
        } else {
            send_len = send_buf_index;
        }

        Log_dump("post data", (char *)send_buf, send_len);

        if (iot_llsync_send(uplink_data->char_uuid, send_buf, send_len)) {
            Log_e("event(type: %d) post failed, len: %d", uplink_data->type, send_len);
            return QCLOUD_ERR_FAILURE;
        }
    } while (left_len != 0);

    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief report device info
 *
 * @param type device name or version
 * @return 0 for success
 */
int ble_uplink_report_device_info(uint8_t type)
{
    char device_info[56] = {0};  // 1 byte llsync proto version + 2 bytes mtu size + 1 byte length of develop version
    uint16_t mtu_size    = 0;

#if BLE_QTCIOT_REMOTE_SET_MTU
    mtu_size = LLSYNC_MTU_SET_MASK;
#endif  // BLE_QTCIOT_REMOTE_SET_MTU
    mtu_size |= ATT_USER_MTU;
    mtu_size       = HTONS(mtu_size);
    device_info[0] = BLE_QTCIOT_LLSYNC_PROTOCOL_VERSION;
    memcpy(&device_info[1], &mtu_size, sizeof(mtu_size));
    if (type == E_REPORT_DEVNAME) {
        device_info[3] = (char)ble_get_device_name(&device_info[4]);
    } else {
        device_info[3] = (char)ble_get_device_version(&device_info[4]);
    }

    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_REPORT_MTU;
    uplink_data.buf              = (const char *)device_info;
    uplink_data.buf_len          = 4 + device_info[3];
    return ble_uplink_notify2(&uplink_data);
}

/**
 * @brief sync mtu to wechat mini app
 *
 * @param att_mtu new mtu
 * @return 0 for success
 */
int ble_uplink_sync_mtu(uint16_t att_mtu)
{
    uint16_t mtu_size = 0;

    mtu_size = ATT_MTU_TO_LLSYNC_MTU(att_mtu);
    llsync_mtu_update(mtu_size);
    mtu_size = HTONS(mtu_size);

    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_SYNC_MTU;
    uplink_data.buf              = (const char *)&mtu_size;
    uplink_data.buf_len          = sizeof(uint16_t);
    return ble_uplink_notify2(&uplink_data);
}

#if BLE_QTCIOT_LLSYNC_CONFIG_NET
/**
 * @brief report result
 *
 * @param[in] type report type
 * @param[in] result report result
 * @return 0 for success
 */
static int ble_uplink_report_result(uint8_t type, uint8_t result)
{
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = type;
    uplink_data.buf              = (const char *)&result;
    uplink_data.buf_len          = sizeof(uint8_t);
    return ble_uplink_notify2(&uplink_data);
}

/**
 * @brief report wifi connect
 *
 * @param[in] mode wifi mode(STA/AP)
 * @param[in] state wifi state
 * @param[in] ssid wifi ssid buffer
 * @param[in] ssid_len wifi ssid buffer length
 * @return 0 for success
 */
int ble_uplink_report_wifi_connect(uint8_t mode, uint8_t state, const char *ssid, uint8_t ssid_len)
{
    char    buf[48] = {0};
    uint8_t pos     = 0;

    buf[pos++] = mode;
    buf[pos++] = state;
    buf[pos++] = 0;
    buf[pos++] = ssid_len;
    memcpy(buf + pos, ssid, ssid_len);

    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_WIFI_CONNECT;
    uplink_data.buf              = (const char *)buf;
    uplink_data.buf_len          = pos + ssid_len;
    return ble_uplink_notify2(&uplink_data);
}

/**
 * @brief report wifi log
 *
 * @param[in] log wifi log buffer
 * @param[in] log_size wifi log buffer length
 * @return 0 for success
 */
int ble_uplink_report_wifi_log(const uint8_t *log, uint16_t log_size)
{
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_WIFI_LOG;
    uplink_data.buf              = (const char *)log;
    uplink_data.buf_len          = log_size;
    return ble_uplink_notify2(&uplink_data);
}
#endif  // BLE_QTCIOT_LLSYNC_CONFIG_NET

#if BLE_QTCIOT_LLSYNC_STANDARD
/**
 * @brief get status
 *
 * @return 0 for success
 */
int ble_uplink_get_status(void)
{
#ifdef BLE_QTCIOT_INCLUDE_PROPERTY
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_GET_STATUS;
    return ble_uplink_notify2(&uplink_data);
#else
    Log_e("property" BLE_QTCIOT_NOT_SUPPORT_WARN);
    return QCLOUD_RET_SUCCESS;
#endif
}

#if BLE_QTCIOT_SECURE_BIND
/**
 * @brief update wait time
 *
 * @param time unit:s
 * @return 0 for success
 */
int ble_uplink_sync_wait_time(uint16_t time)
{
    time                         = HTONS(time);
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_SYNC_WAIT_TIME;
    uplink_data.buf              = (const char *)&time;
    uplink_data.buf_len          = sizeof(uint16_t);
    return ble_uplink_notify2(&uplink_data);
}
#endif  // BLE_QTCIOT_SECURE_BIND

#endif  // BLE_QTCIOT_LLSYNC_STANDARD

// ---------------------------------------------------------------------------------------------
// downlink data handle
// ---------------------------------------------------------------------------------------------
typedef struct {
    IotBool  have_data;  // start received package
    uint8_t  type;       // event type
    uint16_t buf_len;    // the length of data
    char     buf[BLE_QTCIOT_EVENT_MAX_SIZE];
} BLELLsyncSlice;

// llsync support data fragment, so we need to package all the data before parsing if the data is slice
static BLELLsyncSlice sg_ble_slice_data;

#if BLE_QTCIOT_BUTTON_BROADCAST
static ble_timer_t sg_bind_timer = NULL;
#endif  // BLE_QTCIOT_BUTTON_BROADCAST

#if BLE_QTCIOT_LLSYNC_STANDARD

#if BLE_QTCIOT_SECURE_BIND
static BLELLsyncBindData sg_bind_auth_data;
/**
 * @brief handle secure bind
 *
 * @param data wechat mini app secure bind data buffer
 * @param len wechat mini app secure bind data buffer length
 * @return int
 */
static int ble_secure_bind_handle(const char *data, uint16_t len)
{
    memset(&sg_bind_auth_data, 0, sizeof(BLELLsyncBindData));
    memcpy(&sg_bind_auth_data, data, sizeof(BLELLsyncBindData));
    ble_uplink_sync_wait_time(BLE_QTCIOT_BIND_WAIT_TIME);
    if (sg_iot_ble_llsync.callback.secure_bind_callback) {
        sg_iot_ble_llsync.callback.secure_bind_callback(sg_iot_ble_llsync.usr_data);
    }
    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief secure bind confirm
 *
 * @param choose @see BLELLsyncSecureBindState
 * @return 0 for success
 */
int ble_secure_bind_user_confirm(BLELLsyncSecureBindState choose)
{
    char    out_buf[80] = {0};
    int     rc_len      = 0;
    uint8_t flag        = 0;

    Log_i("user choose: %d", choose);
    rc_len = ble_bind_get_authcode(&sg_bind_auth_data, sizeof(BLELLsyncBindData), out_buf, sizeof(out_buf));
    if (rc_len <= 0) {
        Log_e("get bind authcode failed");
        return QCLOUD_ERR_FAILURE;
    }
    flag = choose << 5;
    return ble_uplink_notify2((uint8_t)BLE_QTCIOT_EVENT_UP_BIND_SIGN_RET, flag, NULL, 0, out_buf, rc_len);
}
#endif  // BLE_QTCIOT_SECURE_BIND

#endif  // BLE_QTCIOT_LLSYNC_STANDARD

#if BLE_QTCIOT_BUTTON_BROADCAST
/**
 * @brief bind timer timeout callback
 *
 * @param usr_data
 */
static void ble_bind_timer_callback(void *usr_data)
{
    Log_i("timer timeout");
    if (E_LLSYNC_BIND_WAIT == llsync_bind_state_get()) {
        ble_advertising_stop();
        llsync_bind_state_set(E_LLSYNC_BIND_IDLE);
        Log_i("stop advertising");
    }
}
#endif  // BLE_QTCIOT_BUTTON_BROADCAST

static uint8_t ble_msg_type_header_len(uint8_t type)
{
    if (type == BLE_QTCIOT_GET_STATUS_REPLY_DATA_TYPE) {
        return BLE_QTCIOT_GET_STATUS_REPLY_HEADER_LEN;
    } else {
        return BLE_QTCIOT_DATA_FIXED_HEADER_LEN;
    }
}

static int8_t ble_package_slice_data(uint8_t data_type, uint8_t flag, uint8_t header_len, const char *in_buf,
                                     int in_len)
{
    if (!BLE_QTCIOT_IS_SLICE_HEADER(flag)) {
        if (!sg_ble_slice_data.have_data) {
            Log_e("slice no header");
            return -1;
        }
        if (data_type != sg_ble_slice_data.type) {
            Log_e("msg type: %d != %d", data_type, sg_ble_slice_data.type);
            return -1;
        }
        if (sg_ble_slice_data.buf_len + (in_len - header_len) > sizeof(sg_ble_slice_data.buf)) {
            Log_e("too long data: %d > %d", sg_ble_slice_data.buf_len + (in_len - header_len),
                  sizeof(sg_ble_slice_data.buf));
            return -1;
        }
    }

    if (BLE_QTCIOT_IS_SLICE_HEADER(flag)) {
        if (sg_ble_slice_data.have_data) {
            Log_i("new data coming, clean the package buffer");
            memset(&sg_ble_slice_data, 0, sizeof(sg_ble_slice_data));
        }
        sg_ble_slice_data.have_data = TCIOT_BOOL_TRUE;
        sg_ble_slice_data.type      = data_type;
        // reserved space for payload length field
        sg_ble_slice_data.buf_len += header_len;
        sg_ble_slice_data.buf[0] = in_buf[0];
        memcpy(sg_ble_slice_data.buf + sg_ble_slice_data.buf_len, in_buf + header_len, in_len - header_len);
        sg_ble_slice_data.buf_len += (in_len - header_len);
        return 1;
    } else if (BLE_QTCIOT_IS_SLICE_BODY(flag)) {
        memcpy(sg_ble_slice_data.buf + sg_ble_slice_data.buf_len, in_buf + header_len, in_len - header_len);
        sg_ble_slice_data.buf_len += (in_len - header_len);
        return 1;
    } else {
        memcpy(sg_ble_slice_data.buf + sg_ble_slice_data.buf_len, in_buf + header_len, in_len - header_len);
        sg_ble_slice_data.buf_len += (in_len - header_len);
        return 0;
    }
}

/**
 * @brief llsync data characteristic uuid : 0xffe1
 *
 * @param[in] in_buf llsync data buffer
 * @param[in] in_len llsync data buffer length
 * @return 0 for success
 */
static int ble_device_info_msg_handle(const char *in_buf, int in_len)
{
    POINTER_SANITY_CHECK(in_buf, QCLOUD_ERR_INVAL);
    uint8_t          ch;
    char            *p_data      = NULL;
    int              p_data_len  = 0;
    uint16_t         tmp_len     = 0;
    uint8_t          header_len  = 0;
    int              rc          = QCLOUD_RET_SUCCESS;

#if BLE_QTCIOT_LLSYNC_CONFIG_NET
    static char    ssid[64] = {0};
    static uint8_t ssid_len;
    char          *p_ssid   = NULL;
    char          *p_passwd = NULL;
#endif
    // This flag is use to avoid attacker jump "ble_conn_get_authcode()" step, then
    // send 'E_DEV_MSG_CONN_SUCC' msg, and device straightly set 'E_LLSYNC_CONNECTED' flag.
    // This behavior make signature check useless lead to risk.
#if BLE_QTCIOT_LLSYNC_STANDARD
    char             out_buf[80] = {0};
    int              rc_len      = 0;
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    static IotBool conn_flag = TCIOT_BOOL_FALSE;
#endif

    p_data     = (char *)in_buf;
    p_data_len = in_len;

    // E_DEV_MSG_SYNC_TIME, E_DEV_MSG_CONN_VALID, E_DEV_MSG_BIND_SUCC, E_DEV_MSG_UNBIND this 4 type
    // of message has more than one bytes data, it may cut to several slices, here need to merge them
    // together, other type message only has 1 byte data, not need merge.
    if ((in_len > 3) && BLE_QTCIOT_IS_SLICE_PACKAGE(in_buf[1])) {
        // Log_dump( "slice", p_data, p_data_len);
        header_len = ble_msg_type_header_len(in_buf[0]);
        rc         = ble_package_slice_data(in_buf[0], in_buf[1], header_len, in_buf, in_len);
        if (rc < 0) {
            return QCLOUD_ERR_FAILURE;
        } else if (rc == 0) {
            //tmp_len = HTONS(sg_ble_slice_data.buf_len - header_len);
            {
                uint16_t tmp = sg_ble_slice_data.buf_len - header_len;
                tmp_len = HTONS(tmp);
            }
            memcpy(&sg_ble_slice_data.buf[1], &tmp_len, sizeof(tmp_len));
            p_data     = sg_ble_slice_data.buf;
            p_data_len = sg_ble_slice_data.buf_len;
        } else if (rc > 0) {
            return QCLOUD_RET_SUCCESS;
        }
    }
    Log_dump("tlv", p_data, p_data_len);
    rc = 2;  // default fail.
    ch = p_data[0];
    switch (ch) {
#if BLE_QTCIOT_LLSYNC_STANDARD
        case E_DEV_MSG_SYNC_TIME:
#if BLE_QTCIOT_DYNREG_ENABLE
            if (is_llsync_need_dynreg()) {
                rc_len = ble_dynreg_get_authcode(p_data + 3, p_data_len - 3, out_buf, sizeof(out_buf));
                if (rc_len <= 0) {
                    Log_e("get dynreg authcode failed");
                    rc = QCLOUD_ERR_FAILURE;
                    break;
                }
                uplink_data.type    = BLE_QTCIOT_EVENT_UP_DYNREG_SIGN;
                uplink_data.buf     = out_buf;
                uplink_data.buf_len = rc_len;
                rc                  = ble_uplink_notify2(&uplink_data);
                break;
            }
#endif
#if BLE_QTCIOT_SECURE_BIND
            rc = ble_secure_bind_handle(p_data + 3, p_data_len - 3);
#else
            rc_len = ble_bind_get_authcode(p_data + 3, p_data_len - 3, out_buf, sizeof(out_buf));
            if (rc_len <= 0) {
                Log_e("get bind authcode failed");
                rc = QCLOUD_ERR_FAILURE;
                break;
            }

            uplink_data.type    = BLE_QTCIOT_EVENT_UP_BIND_SIGN_RET;
            uplink_data.buf     = out_buf;
            uplink_data.buf_len = rc_len;
            rc                  = ble_uplink_notify2(&uplink_data);
#endif  // BLE_QTCIOT_SECURE_BIND
            break;
        case E_DEV_MSG_CONN_VALID:
            rc_len = ble_conn_get_authcode(p_data + 3, p_data_len - 3, out_buf, sizeof(out_buf));
            if (rc_len <= 0) {
                Log_e("get connect authcode failed");
                rc = QCLOUD_ERR_FAILURE;
                break;
            }
            uplink_data.type    = BLE_QTCIOT_EVENT_UP_CONN_SIGN_RET;
            uplink_data.buf     = out_buf;
            uplink_data.buf_len = rc_len;
            rc                  = ble_uplink_notify2(&uplink_data);
            conn_flag           = TCIOT_BOOL_TRUE;
            break;
        case E_DEV_MSG_BIND_SUCC:
            if (QCLOUD_RET_SUCCESS != ble_bind_write_result(p_data + 3, p_data_len - 3)) {
                Log_e("write bind result failed");
                rc = QCLOUD_ERR_FAILURE;
            }
            break;
        case E_DEV_MSG_BIND_FAIL:
            Log_i("get msg bind fail");
            break;
        case E_DEV_MSG_UNBIND:
            rc_len = ble_unbind_get_authcode(p_data + 3, p_data_len - 3, out_buf, sizeof(out_buf));
            if (rc_len <= 0) {
                Log_e("get unbind authcode failed");
                rc = QCLOUD_ERR_FAILURE;
                break;
            }
            uplink_data.type    = BLE_QTCIOT_EVENT_UP_UNBIND_SIGN_RET;
            uplink_data.buf     = out_buf;
            uplink_data.buf_len = rc_len;
            rc                  = ble_uplink_notify2(&uplink_data);
            break;
        case E_DEV_MSG_CONN_SUCC:
            if (!conn_flag) {
                break;
            }
            conn_flag = TCIOT_BOOL_FALSE;
            Log_i("get msg connect success");
            llsync_connection_state_set(E_LLSYNC_CONNECTED);
            rc = ble_uplink_report_device_info(E_REPORT_DEVINFO);
            break;
        case E_DEV_MSG_CONN_FAIL:
            Log_i("get msg connect fail");
            break;
        case E_DEV_MSG_UNBIND_SUCC:
            Log_i("get msg unbind success");
            if (QCLOUD_RET_SUCCESS != ble_unbind_write_result()) {
                Log_e("write unbind result failed");
                rc = QCLOUD_ERR_FAILURE;
            }
            break;
        case E_DEV_MSG_UNBIND_FAIL:
            Log_i("get msg unbind fail");
            break;
        case E_DEV_MSG_BIND_TIMEOUT:
            Log_i("get msg bind result: %d", p_data[1]);
#if (1 == BLE_QTCIOT_SECURE_BIND)
            ble_secure_bind_user_notify(p_data[1]);
#endif
            break;
#if BLE_QTCIOT_DYNREG_ENABLE
        case E_DEV_MSG_DYNREG:
            rc = ble_dynreg_parse_psk(p_data + 5, p_data[4]);
            if (rc < 0) {
                break;
            }
#if BLE_QTCIOT_SECURE_BIND
            rc = ble_secure_bind_handle(p_data + 5 + p_data[4], p_data_len - 5 - p_data[4]);
#else
            rc_len =
                ble_bind_get_authcode(p_data + 5 + p_data[4], p_data_len - 5 - p_data[4], out_buf, sizeof(out_buf));
            if (rc_len <= 0) {
                Log_e("get bind authcode failed");
                rc = QCLOUD_ERR_FAILURE;
                break;
            }
            uplink_data.type    = BLE_QTCIOT_EVENT_UP_BIND_SIGN_RET;
            uplink_data.buf     = out_buf;
            uplink_data.buf_len = rc_len;
            rc                  = ble_uplink_notify2(&uplink_data);
#endif  // BLE_QTCIOT_SECURE_BIND
            break;
#endif  // BLE_QTCIOT_DYNREG_ENABLE
#endif  // BLE_QTCIOT_LLSYNC_STANDARD

#if BLE_QTCIOT_LLSYNC_CONFIG_NET
        case E_DEV_MSG_GET_DEV_INFO:
            llsync_connection_state_set(E_LLSYNC_CONNECTED);
            rc = ble_uplink_report_device_info(E_REPORT_DEVNAME);
            break;
        case E_DEV_MSG_SET_WIFI_MODE:
            if (sg_iot_ble_llsync.wifi_callback.set_wifi_mode) {
                rc = sg_iot_ble_llsync.wifi_callback.set_wifi_mode(p_data[1], sg_iot_ble_llsync.wifi_usr_data);
                rc = ble_uplink_report_result(BLE_QTCIOT_EVENT_UP_WIFI_MODE, rc);
            }
            break;
        case E_DEV_MSG_SET_WIFI_INFO:
            // 1 byte ssid len + N bytes ssid + 1 byte pwd len + N bytes pwd
            p_ssid   = &p_data[3];
            ssid_len = p_data[3];
            memcpy(ssid, &p_data[4], ssid_len);
            p_passwd = &p_ssid[p_ssid[0] + 1];
            if (sg_iot_ble_llsync.wifi_callback.set_wifi_info) {
                rc = sg_iot_ble_llsync.wifi_callback.set_wifi_info(ssid, ssid_len, &p_passwd[1], p_passwd[0],
                                                                   sg_iot_ble_llsync.wifi_usr_data);
                rc = ble_uplink_report_result(BLE_QTCIOT_EVENT_UP_WIFI_INFO, rc);
            }
            break;
        case E_DEV_MSG_SET_WIFI_CONNECT:
            if (sg_iot_ble_llsync.wifi_callback.connect_wifi) {
                rc = sg_iot_ble_llsync.wifi_callback.connect_wifi(60000, sg_iot_ble_llsync.wifi_usr_data);
                if (!rc) {
                    rc = ble_uplink_report_wifi_connect(BLE_WIFI_MODE_STA, BLE_WIFI_STATE_CONNECT, ssid, ssid_len);
                } else {
                    rc = ble_uplink_report_wifi_connect(BLE_WIFI_MODE_STA, BLE_WIFI_STATE_OTHER, ssid, ssid_len);
                }
            }
            break;
        case E_DEV_MSG_SET_WIFI_TOKEN:
            if (sg_iot_ble_llsync.wifi_callback.set_wifi_token) {
                rc = sg_iot_ble_llsync.wifi_callback.set_wifi_token(p_data + 3, p_data_len - 3,
                                                                    sg_iot_ble_llsync.wifi_usr_data);
                rc = ble_uplink_report_result(BLE_QTCIOT_EVENT_UP_WIFI_TOKEN, rc);
            }
            break;
        case E_DEV_MSG_GET_DEV_LOG:
            if (sg_iot_ble_llsync.wifi_callback.get_wifi_log) {
                rc = sg_iot_ble_llsync.wifi_callback.get_wifi_log(sg_iot_ble_llsync.wifi_usr_data);
            }
            break;
#endif
        case E_DEV_MSG_SET_MTU_RESULT:
            ble_inform_mtu_result(p_data + 1, p_data_len - 1);
            break;
        default:
            Log_e("unknow type %d", ch);
            break;
    }
    memset(&sg_ble_slice_data, 0, sizeof(sg_ble_slice_data));
    return rc;
}

#if BLE_QTCIOT_LLSYNC_STANDARD
/**
 * @brief llsync data characteristic uuid : 0xffe2
 *
 * @param[in] in_buf llsync data buffer
 * @param[in] in_len llsync data buffer length
 * @return 0 for success
 */
static int ble_lldata_msg_handle(const char *in_buf, int in_len)
{
    POINTER_SANITY_CHECK(in_buf, QCLOUD_ERR_INVAL);

    uint8_t  data_type   = 0;
    uint8_t  data_effect = 0;
    uint8_t  id          = 0;
    uint8_t  slice_flag  = 0;
    uint8_t  header_len  = 0;
    uint8_t  slice_type  = 0;
    uint16_t tmp_len     = 0;
    char    *p_data      = NULL;
    int      p_data_len  = 0;
    int      rc          = 0;

    if (!llsync_is_connected()) {
        Log_e("operation negate, device not connected");
        return QCLOUD_ERR_FAILURE;
    }

    p_data     = (char *)in_buf;
    p_data_len = in_len;

    data_type = BLE_QTCIOT_PARSE_MSG_HEAD_TYPE(in_buf[0]);
    if (data_type >= BLE_QTCIOT_DATA_TYPE_BUTT) {
        Log_e("invalid data type: %d", data_type);
        return QCLOUD_ERR_FAILURE;
    }
    data_effect = BLE_QTCIOT_PARSE_MSG_HEAD_EFFECT(in_buf[0]);
    if (data_effect >= BLE_QTCIOT_EFFECT_BUTT) {
        Log_e("invalid data eff: ect");
        return QCLOUD_ERR_FAILURE;
    }
    id = BLE_QTCIOT_PARSE_MSG_HEAD_ID(in_buf[0]);
    Log_d("data type: %d, effect: %d, id: %d", data_type, data_effect, id);

    // if data is action_reply, control or get_status_reply, the data maybe need package
    if ((data_type == BLE_QTCIOT_MSG_TYPE_ACTION) || (in_buf[0] == BLE_QTCIOT_CONTROL_DATA_TYPE) ||
        (in_buf[0] == BLE_QTCIOT_GET_STATUS_REPLY_DATA_TYPE)) {
        slice_flag = (in_buf[0] == BLE_QTCIOT_GET_STATUS_REPLY_DATA_TYPE) ? in_buf[2] : in_buf[1];
        slice_type = (in_buf[0] == BLE_QTCIOT_GET_STATUS_REPLY_DATA_TYPE) ? in_buf[0] : data_type;

        // Log_dump( "slice", p_data, p_data_len);
        if (BLE_QTCIOT_IS_SLICE_PACKAGE(slice_flag)) {
            header_len = ble_msg_type_header_len(slice_type);
            rc         = ble_package_slice_data(slice_type, slice_flag, header_len, in_buf, in_len);
            if (rc < 0) {
                return QCLOUD_ERR_FAILURE;
            } else if (rc == 0) {
                tmp_len = HTONS(sg_ble_slice_data.buf_len - header_len);
                if (BLE_QTCIOT_GET_STATUS_REPLY_DATA_TYPE == slice_type) {
                    sg_ble_slice_data.buf[1] = in_buf[1];
                    memcpy(&sg_ble_slice_data.buf[2], &tmp_len, sizeof(tmp_len));
                } else {
                    memcpy(&sg_ble_slice_data.buf[1], &tmp_len, sizeof(tmp_len));
                }
                p_data     = sg_ble_slice_data.buf;
                p_data_len = sg_ble_slice_data.buf_len;
            } else if (rc > 0) {
                return QCLOUD_RET_SUCCESS;
            }
        }
    }
#ifdef BLE_QTCIOT_LLSYNC_GATEWAY
    rc = iot_llsync_self_down_data_handle((uint8_t *)p_data, p_data_len);
 #endif //  BLE_QTCIOT_LLSYNC_GATEWAY
    memset(&sg_ble_slice_data, 0, sizeof(sg_ble_slice_data));
    return rc;
}

// ---------------------------------------------------------------------------------
// ble ota
// ---------------------------------------------------------------------------------
#if BLE_QTCIOT_SUPPORT_OTA

static int ble_ota_init(void)
{
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.get_download_addr, QCLOUD_ERR_INVAL);
    sg_iot_ble_llsync.ota.download_address =
        sg_iot_ble_llsync.callback.ota_callback.get_download_addr(sg_iot_ble_llsync.usr_data);

#if BLE_QTCIOT_SUPPORT_RESUMING
    uint32_t size_align   = 0;
    uint8_t  file_percent = 0;
    // start from 0 if read flash fail, but ota will continue so ignored the return code
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.read_flash, QCLOUD_ERR_INVAL);
    sg_iot_ble_llsync.callback.ota_callback.read_flash(sg_iot_ble_llsync.usr_data, BLE_QTCIOT_OTA_INFO_FLASH_ADDR,
                                                       (uint8_t *)&sg_iot_ble_llsync.ota.ota_info_record,
                                                       sizeof(BleOtaInfoRecord));
    // ble_qiot_log_hex(BLE_QTCIOT_LOG_LEVEL_INFO, "ota info", &sg_ota_info, sizeof(sg_ota_info));
    //  check if the valid flag legalled
    if ((sg_iot_ble_llsync.ota.ota_info_record.valid_flag == BLE_QTCIOT_OTA_PAGE_VALID_VAL) &&
        (sg_iot_ble_llsync.ota.download_address == sg_iot_ble_llsync.ota.ota_info_record.last_address)) {
        // the ota info write to flash may be mismatch the file write to flash, we should download file from the byte
        // align the flash page
        size_align = sg_iot_ble_llsync.ota.ota_info_record.last_file_size / BLE_QTCIOT_RECORD_FLASH_PAGESIZE *
                     BLE_QTCIOT_RECORD_FLASH_PAGESIZE;
        sg_iot_ble_llsync.ota.download_file_size += size_align;
        file_percent = size_align * 100 / sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size;
        sg_iot_ble_llsync.ota.download_percent = file_percent;
        Log_i("align file size: %x, the percent: %d", size_align, file_percent);
    }
#endif  // BLE_QTCIOT_SUPPORT_RESUMING
    // malloc ota buffer
    // sg_iot_ble_llsync.ota.data_buf = TCI_HAL_Malloc(BLE_QTCIOT_OTA_BUF_SIZE);
    // if (!sg_iot_ble_llsync.ota.data_buf) {
    //     Log_e("malloc ota data buf fail.");
    //     return QCLOUD_ERR_MALLOC;
    // }
    return QCLOUD_RET_SUCCESS;
}

static int ble_ota_write_info(void)
{
    int rc = 0;
#if BLE_QTCIOT_SUPPORT_RESUMING
    sg_iot_ble_llsync.ota.ota_info_record.valid_flag = ~BLE_QTCIOT_OTA_PAGE_VALID_VAL;
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.write_flash, QCLOUD_ERR_INVAL);
    rc = sg_iot_ble_llsync.callback.ota_callback.write_flash(sg_iot_ble_llsync.usr_data, BLE_QTCIOT_OTA_INFO_FLASH_ADDR,
                                                             (uint8_t *)&sg_iot_ble_llsync.ota.ota_info_record,
                                                             sizeof(BleOtaInfoRecord));
    if (rc) {
        Log_e("record ota download info fail. rc : %d", rc);
        return rc;
    }
#endif  // BLE_QTCIOT_SUPPORT_RESUMING
    return rc;
}

static int ble_ota_write_data_to_flash(void)
{
    int      rc         = 0;
    uint32_t write_addr = sg_iot_ble_llsync.ota.download_address + sg_iot_ble_llsync.ota.download_file_size -
                          sg_iot_ble_llsync.ota.data_buf_size;
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.write_flash, QCLOUD_ERR_INVAL);
    rc = sg_iot_ble_llsync.callback.ota_callback.write_flash(
        sg_iot_ble_llsync.usr_data, write_addr, sg_iot_ble_llsync.ota.data_buf, sg_iot_ble_llsync.ota.data_buf_size);
    if (rc) {
        Log_e("write to [0x%08x] [%d] bytes data fial. rc : %d", write_addr, sg_iot_ble_llsync.ota.data_buf_size, rc);
        return rc;
    }
    return rc;
}

// stop ota if ble disconnect
static int ble_ota_stop(void)
{
    int rc = 0;
    if (!(sg_iot_ble_llsync.ota.ota_flag & BLE_QTCIOT_OTA_REQUEST_BIT)) {
        Log_w("ota not start");
        return rc;
    }
    sg_iot_ble_llsync.ota.ota_flag = 0;
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.delete_ota_timer, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.ota_stop, QCLOUD_ERR_INVAL);
    sg_iot_ble_llsync.callback.ota_callback.delete_ota_timer(sg_iot_ble_llsync.usr_data,
                                                             sg_iot_ble_llsync.ota.ota_timer);

    // write data to flash if the ota stop
    rc = ble_ota_write_data_to_flash();
    rc |= ble_ota_write_info();
    // inform user ota failed because ble disconnect
    sg_iot_ble_llsync.callback.ota_callback.ota_stop(sg_iot_ble_llsync.usr_data, BLE_QTCIOT_OTA_DISCONNECT);
    // TCI_HAL_Free(sg_iot_ble_llsync.ota.data_buf);
    // sg_iot_ble_llsync.ota.data_buf = NULL;
    return rc;
}

static uint8_t ble_ota_type_header_len(uint8_t type)
{
    return BLE_QTCIOT_GET_OTA_REQUEST_HEADER_LEN;
}

static int ble_ota_reply_ota_data(BleOta *ota)
{
    uint8_t  req       = sg_iot_ble_llsync.ota.next_seq;
    uint32_t file_size = ota->download_file_size;

    if (ota->ota_reply_info.file_size != file_size) {
        ota->ota_reply_info.file_size = file_size;
        ota->ota_reply_info.req       = req;
    } else {
        // in the case that the device reply info missed, the timer reply info again but the seq increased, so we need
        // saved the last reply info and used in the time
        req = ota->ota_reply_info.req;
        // ble_qiot_log_d("used old file size %d, req %d", file_size, req);
    }

    file_size = HTONL(file_size);

    // uplink
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_REPLY_OTA_DATA;
    uplink_data.header           = &req;
    uplink_data.header_len       = sizeof(uint8_t);
    uplink_data.buf              = (const char *)&file_size;
    uplink_data.buf_len          = sizeof(uint32_t);
    return ble_uplink_notify2(&uplink_data);
}

static void ble_ota_timer_callback(void *timer)
{
    if (sg_iot_ble_llsync.ota.ota_flag & BLE_QTCIOT_OTA_RECV_DATA_BIT) {
        sg_iot_ble_llsync.ota.ota_flag &= (~BLE_QTCIOT_OTA_RECV_DATA_BIT);
    }
    sg_iot_ble_llsync.ota.timeout_cnt++;
    Log_i("reply in the timer. count : %d", sg_iot_ble_llsync.ota.timeout_cnt);
    ble_ota_reply_ota_data(&sg_iot_ble_llsync.ota);
    if (sg_iot_ble_llsync.ota.timeout_cnt >= BLE_QTCIOT_OTA_MAX_RETRY_COUNT) {
        sg_iot_ble_llsync.ota.ota_flag = 0;
        POINTER_SANITY_CHECK_RTN(sg_iot_ble_llsync.callback.ota_callback.delete_ota_timer);
        POINTER_SANITY_CHECK_RTN(sg_iot_ble_llsync.callback.ota_callback.ota_stop);
        sg_iot_ble_llsync.callback.ota_callback.delete_ota_timer(sg_iot_ble_llsync.usr_data,
                                                                 sg_iot_ble_llsync.ota.ota_timer);
        sg_iot_ble_llsync.callback.ota_callback.ota_stop(sg_iot_ble_llsync.usr_data, BLE_QTCIOT_OTA_ERR_TIMEOUT);
    }
    return;
}

static int ble_ota_request_handle(const char *in_buf, int buf_len)
{
    BUFF_LEN_SANITY_CHECK(buf_len, sizeof(BleOtaFileInfo) - BLE_QTCIOT_OTA_MAX_VERSION_STR, QCLOUD_ERR_INVAL);

    int             rc          = 0;
    uint8_t         reply_flag  = 0;
    uint32_t        file_size   = 0;
    uint32_t        file_crc    = 0;
    uint8_t         version_len = 0;
    char           *p           = (char *)in_buf;
    char           *notify_data = NULL;
    uint16_t        notify_len  = 0;
    BleOtaReplyInfo ota_reply_info;

    rc = ble_ota_init();
    if (rc) {
        Log_e("ble ota init fail. rc : %d", rc);
        return rc;
    }

    memcpy(&file_size, p, sizeof(file_size));
    p += sizeof(file_size);
    memcpy(&file_crc, p, sizeof(file_crc));
    p += sizeof(file_crc);
    version_len    = *p++;
    file_size      = NTOHL(file_size);
    file_crc       = NTOHL(file_crc);
    p[version_len] = '\0';  // version end
    Log_i("request ota, size: %x, crc: %x, version: %.*s", file_size, file_crc, version_len, p);

    // check if the ota is allowed
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.allow_ota, QCLOUD_ERR_INVAL);
    rc = sg_iot_ble_llsync.callback.ota_callback.allow_ota(sg_iot_ble_llsync.usr_data, (const char *)p);
    if (BLE_OTA_ENABLE == rc) {
        reply_flag                      = BLE_QTCIOT_OTA_ENABLE;
        ota_reply_info.package_nums     = BLE_QTCIOT_TOTAL_PACKAGES;
        ota_reply_info.package_size     = BLE_QTCIOT_PACKAGE_LENGTH + BLE_QTCIOT_OTA_DATA_HEADER_LEN;
        ota_reply_info.retry_timeout    = BLE_QTCIOT_RETRY_TIMEOUT;
        ota_reply_info.reboot_timeout   = BLE_QTCIOT_REBOOT_TIME;
        ota_reply_info.last_file_size   = 0;
        ota_reply_info.package_interval = BLE_QTCIOT_PACKAGE_INTERVAL;
#if BLE_QTCIOT_SUPPORT_RESUMING
        reply_flag |= BLE_QTCIOT_OTA_RESUME_ENABLE;
        // check file crc to determine its the same file, download the new file if its different
        if (sg_iot_ble_llsync.ota.ota_info_record.valid_flag == BLE_QTCIOT_OTA_PAGE_VALID_VAL &&
            file_crc == sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_crc &&
            file_size == sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size) {
            ota_reply_info.last_file_size = HTONL(sg_iot_ble_llsync.ota.download_file_size);
        }
#endif  // BLE_QTCIOT_SUPPORT_RESUMING

        sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size = file_size;
        sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_crc  = file_crc;
        memcpy(sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_version, p, version_len);
        if (sg_iot_ble_llsync.callback.ota_callback.ota_start) {
            sg_iot_ble_llsync.callback.ota_callback.ota_start(sg_iot_ble_llsync.usr_data);
        }

        // start ble ota timer
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.create_ota_timer, QCLOUD_ERR_INVAL);
        sg_iot_ble_llsync.ota.ota_timer = sg_iot_ble_llsync.callback.ota_callback.create_ota_timer(
            sg_iot_ble_llsync.usr_data, ble_ota_timer_callback);
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.ota.ota_timer, QCLOUD_ERR_INVAL);
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.start_ota_timer, QCLOUD_ERR_INVAL);
        rc = sg_iot_ble_llsync.callback.ota_callback.start_ota_timer(
            sg_iot_ble_llsync.usr_data, sg_iot_ble_llsync.ota.ota_timer, BLE_QTCIOT_RETRY_TIMEOUT * 1000);
        if (rc) {
            Log_e("start ble ota timer fial. rc : %d", rc);
        }

        // handler the ota data after ota request
        sg_iot_ble_llsync.ota.ota_flag |= BLE_QTCIOT_OTA_REQUEST_BIT;
        notify_data = (char *)&ota_reply_info;
        notify_len  = sizeof(BleOtaReplyInfo) - sizeof(ota_reply_info.rsv);
    } else {
        reply_flag &= ~BLE_QTCIOT_OTA_ENABLE;
        notify_data = (char *)&rc;  // ota not alload
        notify_len  = sizeof(uint8_t);
    }
    // uplink
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_REPLY_OTA_REPORT;
    uplink_data.header           = &reply_flag;
    uplink_data.header_len       = sizeof(uint8_t);
    uplink_data.buf              = (const char *)notify_data;
    uplink_data.buf_len          = notify_len;
    return ble_uplink_notify2(&uplink_data);
}

int ble_qiot_ota_data_saved(BleOta *ota, char *data, uint16_t data_len)
{
    int      rc         = 0;
    uint8_t  percent    = 0;
    uint32_t write_addr = 0;

    // write data to flash if the buffer overflow
    if ((data_len + ota->data_buf_size) > sizeof(ota->data_buf)) {
        memcpy(ota->data_buf + ota->data_buf_size, data, sizeof(ota->data_buf) - ota->data_buf_size);
        ota->download_file_size += (sizeof(ota->data_buf) - ota->data_buf_size);
        data += (sizeof(ota->data_buf) - ota->data_buf_size);
        data_len -= (sizeof(ota->data_buf) - ota->data_buf_size);
        ota->data_buf_size += (sizeof(ota->data_buf) - ota->data_buf_size);

        // write data to flash
        write_addr = ota->download_address + ota->download_file_size - ota->data_buf_size;
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.write_flash, QCLOUD_ERR_INVAL);
        rc = sg_iot_ble_llsync.callback.ota_callback.write_flash(sg_iot_ble_llsync.usr_data, write_addr, ota->data_buf,
                                                                 ota->data_buf_size);
        if (rc) {
            Log_e("write to [0x%08x] [%d] bytes data fial. rc : %d", write_addr, ota->data_buf_size, rc);
            return rc;
        }

        // update the ota info if support resuming
        percent =
            (ota->download_file_size + ota->data_buf_size) * 100 / ota->ota_info_record.download_file_info.file_size;
        if (percent > ota->download_percent) {
            ota->download_percent = percent;
            Log_i("download percent : %d", percent);
            // save write info
#if BLE_QTCIOT_SUPPORT_RESUMING
            ota->ota_info_record.valid_flag     = BLE_QTCIOT_OTA_PAGE_VALID_VAL;
            ota->ota_info_record.last_file_size = ota->download_file_size;
            ota->ota_info_record.last_address   = ota->download_address;
            POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.write_flash, QCLOUD_ERR_INVAL);
            rc = sg_iot_ble_llsync.callback.ota_callback.write_flash(
                sg_iot_ble_llsync.usr_data, BLE_QTCIOT_OTA_INFO_FLASH_ADDR, (uint8_t *)&ota->ota_info_record,
                sizeof(BleOtaInfoRecord));
            if (rc) {
                Log_e("record ota download info fail. rc : %d", rc);
                return rc;
            }
#endif  // BLE_QTCIOT_SUPPORT_RESUMING
        }
        memset(ota->data_buf, 0, sizeof(ota->data_buf));
        ota->data_buf_size = 0;
    }

    memcpy(ota->data_buf + ota->data_buf_size, data, data_len);
    ota->data_buf_size += data_len;
    ota->download_file_size += data_len;

    // if the last package, write to flash and reply the server
    if (ota->download_file_size == ota->ota_info_record.download_file_info.file_size) {
        Log_i("receive the last package");
        // write last package
        write_addr = ota->download_address + ota->download_file_size - ota->data_buf_size;
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.write_flash, QCLOUD_ERR_INVAL);
        rc = sg_iot_ble_llsync.callback.ota_callback.write_flash(sg_iot_ble_llsync.usr_data, write_addr, ota->data_buf,
                                                                 ota->data_buf_size);
        if (rc) {
            Log_e("write to [0x%08x] [%d] bytes data fial. rc : %d", write_addr, ota->data_buf_size, rc);
            return rc;
        }
        ble_ota_reply_ota_data(ota);
        // set the file receive end bit
        ota->ota_flag |= BLE_QTCIOT_OTA_RECV_END_BIT;
        memset(ota->data_buf, 0, sizeof(ota->data_buf));
        ota->data_buf_size = 0;
    }

    return rc;
}

static int ble_ota_data_handle(const char *in_buf, int buf_len)
{
    POINTER_SANITY_CHECK(in_buf, QCLOUD_ERR_INVAL);
    int      rc       = 0;
    uint8_t  seq      = 0;
    char    *data     = NULL;
    uint16_t data_len = 0;

    if (!(sg_iot_ble_llsync.ota.ota_flag & BLE_QTCIOT_OTA_REQUEST_BIT)) {
        Log_e("ota request is need first");
        return QCLOUD_ERR_FAILURE;
    }

    seq      = in_buf[0];
    data     = (char *)in_buf + 1;
    data_len = (uint16_t)buf_len - 1;

    // ble_qiot_log_hex(BLE_QTCIOT_LOG_LEVEL_ERR, "data", in_buf, buf_len);
    if (seq == sg_iot_ble_llsync.ota.next_seq) {
        sg_iot_ble_llsync.ota.ota_flag |= BLE_QTCIOT_OTA_RECV_DATA_BIT;
        sg_iot_ble_llsync.ota.ota_flag |= BLE_QTCIOT_OTA_FIRST_RETRY_BIT;
        sg_iot_ble_llsync.ota.timeout_cnt = 0;
        sg_iot_ble_llsync.ota.next_seq++;

        rc = ble_qiot_ota_data_saved(&sg_iot_ble_llsync.ota, data, data_len);
        if (rc) {
            // stop ota and inform the server
            Log_e("stop ota. because save data failed, rc : %d", rc);
            return rc;
        }
        if (sg_iot_ble_llsync.ota.ota_flag & BLE_QTCIOT_OTA_RECV_END_BIT) {
            return QCLOUD_RET_SUCCESS;
        }
        // reply the server if received the last package in the loop
        if (BLE_QTCIOT_TOTAL_PACKAGES == sg_iot_ble_llsync.ota.next_seq) {
            ble_ota_reply_ota_data(&sg_iot_ble_llsync.ota);
            sg_iot_ble_llsync.ota.next_seq = 0;
        }
        sg_iot_ble_llsync.ota.ota_flag |= BLE_QTCIOT_OTA_RECV_DATA_BIT;
    } else {
        // request data only once in the loop, controlled by the flag
        Log_w("unexpect seq %d, expect seq %d", seq, sg_iot_ble_llsync.ota.next_seq);
        if (sg_iot_ble_llsync.ota.ota_flag & BLE_QTCIOT_OTA_FIRST_RETRY_BIT) {
            sg_iot_ble_llsync.ota.ota_flag &= (~BLE_QTCIOT_OTA_FIRST_RETRY_BIT);
            sg_iot_ble_llsync.ota.ota_flag &= (~BLE_QTCIOT_OTA_RECV_DATA_BIT);
            ble_ota_reply_ota_data(&sg_iot_ble_llsync.ota);
            // refresh the timer
            POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.stop_ota_timer, QCLOUD_ERR_INVAL);
            rc = sg_iot_ble_llsync.callback.ota_callback.stop_ota_timer(sg_iot_ble_llsync.usr_data,
                                                                        sg_iot_ble_llsync.ota.ota_timer);
            if (rc) {
                Log_e("stop ble ota timer fial. rc : %d", rc);
            }
            POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.start_ota_timer, QCLOUD_ERR_INVAL);
            rc = sg_iot_ble_llsync.callback.ota_callback.start_ota_timer(
                sg_iot_ble_llsync.usr_data, sg_iot_ble_llsync.ota.ota_timer, BLE_QTCIOT_RETRY_TIMEOUT * 1000);
            if (rc) {
                Log_e("start ble ota timer fial. rc : %d", rc);
            }
        }
    }
    return rc;
}

static inline int ble_ota_report_check_result(uint8_t firmware_valid, uint8_t error_code)
{
    uint8_t result = firmware_valid | error_code;
    // uplink
    LLsyncUpLinkData uplink_data = DEFAULT_LLSYNC_UPLINK_DATA;
    uplink_data.type             = BLE_QTCIOT_EVENT_UP_REPORT_CHECK_RESULT;
    uplink_data.header           = NULL;
    uplink_data.header_len       = 0;
    uplink_data.buf              = (const char *)&result;
    uplink_data.buf_len          = sizeof(uint8_t);
    return ble_uplink_notify2(&uplink_data);
}

/**
 * @brief call the function after the server inform or the device receive the last package
 *
 * @return 0 for success
 */
static int ble_ota_file_end_handle(void)
{
    int      crc_buf_len  = 0;
    uint32_t crc_file_len = 0;
    uint32_t crc          = 0;
    int      rc           = 0;

    // the function called only once in the same ota process
    sg_iot_ble_llsync.ota.ota_flag = 0;
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.delete_ota_timer, QCLOUD_ERR_INVAL);
    rc = sg_iot_ble_llsync.callback.ota_callback.delete_ota_timer(sg_iot_ble_llsync.usr_data,
                                                                  sg_iot_ble_llsync.ota.ota_timer);
    if (rc) {
        Log_e("delete ota timer fial. rc : %d", rc);
    }
    Log_i("calc crc start");
    while (crc_file_len < sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size) {
        crc_buf_len = sizeof(sg_iot_ble_llsync.ota.data_buf) >
                              (sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size - crc_file_len)
                          ? (sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size - crc_file_len)
                          : sizeof(sg_iot_ble_llsync.ota.data_buf);
        memset(sg_iot_ble_llsync.ota.data_buf, 0, sizeof(sg_iot_ble_llsync.ota.data_buf));
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.read_flash, QCLOUD_ERR_INVAL);
        sg_iot_ble_llsync.callback.ota_callback.read_flash(sg_iot_ble_llsync.usr_data,
                                                           sg_iot_ble_llsync.ota.download_address + crc_file_len,
                                                           sg_iot_ble_llsync.ota.data_buf, crc_buf_len);

        crc_file_len += crc_buf_len;
        crc = utils_crc32(crc, (const uint8_t *)sg_iot_ble_llsync.ota.data_buf, crc_buf_len);
        // maybe need task delay
    }

    Log_i("calc crc %x, file crc %x", crc, sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_crc);
    if (crc == sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_crc) {
        POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.valid_file_cb, QCLOUD_ERR_INVAL);
        rc = sg_iot_ble_llsync.callback.ota_callback.valid_file_cb(
            sg_iot_ble_llsync.usr_data, sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_size,
            (const char *)sg_iot_ble_llsync.ota.ota_info_record.download_file_info.file_version);

        if (!rc) {
            ble_ota_report_check_result(BLE_QTCIOT_OTA_VALID_SUCCESS, 0);
            rc = BLE_QTCIOT_OTA_SUCCESS;
        } else {
            ble_ota_report_check_result(BLE_QTCIOT_OTA_VALID_FAIL, BLE_QTCIOT_OTA_FILE_ERROR);
            rc = BLE_QTCIOT_OTA_ERR_FILE;
        }
    } else {
        ble_ota_report_check_result(BLE_QTCIOT_OTA_VALID_FAIL, BLE_QTCIOT_OTA_CRC_ERROR);
        rc = BLE_QTCIOT_OTA_ERR_CRC;
    }
    POINTER_SANITY_CHECK(sg_iot_ble_llsync.callback.ota_callback.ota_stop, QCLOUD_ERR_INVAL);
    sg_iot_ble_llsync.callback.ota_callback.ota_stop(sg_iot_ble_llsync.usr_data, rc);

    ble_ota_write_info();

    return rc;
}

/**
 * @brief llsync data characteristic uuid : 0xffe4
 *
 * @param[in] in_buf llsync data buffer
 * @param[in] in_len llsync data buffer length
 * @return 0 for success
 */
static int ble_ota_msg_handle(const char *buf, uint16_t len)
{
    POINTER_SANITY_CHECK(buf, QCLOUD_ERR_INVAL);

    uint8_t  data_type  = 0;
    int      rc         = QCLOUD_RET_SUCCESS;
    uint8_t  header_len = 0;
    uint8_t  slice_flag = 0;
    char    *p_data     = NULL;
    int      p_data_len = 0;
    uint16_t tmp_len    = 0;

    if (!llsync_is_connected()) {
        Log_e("upgrade forbidden, device not connected");
        return QCLOUD_ERR_FAILURE;
    }

    data_type  = (buf[0] == BLE_QTCIOT_OTA_MSG_REQUEST) ? BLE_QTCIOT_OTA_MSG_REQUEST : buf[0];
    slice_flag = (buf[0] == BLE_QTCIOT_OTA_MSG_REQUEST) ? buf[1] : buf[0];
    if (data_type >= BLE_QTCIOT_OTA_MSG_BUTT) {
        Log_e("invalid data type %d", data_type);
        return QCLOUD_ERR_FAILURE;
    }
    p_data     = (char *)buf;
    p_data_len = len;

    // Log_i("ota data type %d, flag %d", data_type, slice_flag);
    if (BLE_QTCIOT_IS_SLICE_PACKAGE(slice_flag)) {
        Log_dump("tlv", p_data, p_data_len);
        header_len = ble_ota_type_header_len(data_type);
        rc         = ble_package_slice_data(data_type, slice_flag, header_len, buf, len);
        if (rc < 0) {
            return QCLOUD_ERR_FAILURE;
        } else if (rc == 0) {
            if (data_type == BLE_QTCIOT_OTA_MSG_REQUEST) {
                tmp_len = HTONS(sg_ble_slice_data.buf_len - header_len);
                memcpy(&sg_ble_slice_data.buf[1], &tmp_len, sizeof(tmp_len));
            } else {
                sg_ble_slice_data.buf[1] = sg_ble_slice_data.buf_len - header_len;
            }
            if (data_type == BLE_QTCIOT_OTA_MSG_DATA) {
                sg_ble_slice_data.buf[2] = buf[2];
            }
            p_data     = sg_ble_slice_data.buf;
            p_data_len = sg_ble_slice_data.buf_len;
        } else if (rc > 0) {
            return QCLOUD_RET_SUCCESS;
        }
    }

    // Log_dump( "tlv", p_data, p_data_len);
    switch (data_type) {
        case BLE_QTCIOT_OTA_MSG_REQUEST:
            rc = ble_ota_request_handle(p_data + 3, p_data_len);
            break;
        case BLE_QTCIOT_OTA_MSG_DATA:
            rc = ble_ota_data_handle(p_data + 2, p_data_len - 2);
            break;
        case BLE_QTCIOT_OTA_MSG_END:
            rc = ble_ota_file_end_handle();
            break;
        default:
            break;
    }
    memset(&sg_ble_slice_data, 0, sizeof(sg_ble_slice_data));

    return rc;
}

int iot_llsync_get_ota_buffer(uint8_t **buffer, uint32_t *buffer_size)
{
    *buffer      = sg_iot_ble_llsync.ota.data_buf;
    *buffer_size = BLE_QTCIOT_OTA_BUF_SIZE;
    return QCLOUD_RET_SUCCESS;
}

#else
static int ble_ota_msg_handle(const char *buf, uint16_t len)
{
    return QCLOUD_RET_SUCCESS;
}

int iot_llsync_get_ota_buffer(uint8_t **buffer, uint32_t *buffer_size)
{
    *buffer      = NULL;
    *buffer_size = 0;
    return QCLOUD_ERR_MALLOC;
}

#endif  // BLE_QTCIOT_SUPPORT_OTA

#else 

static int ble_lldata_msg_handle(const char *in_buf, int in_len)
{
    return QCLOUD_RET_SUCCESS;
}

static int ble_ota_msg_handle(const char *buf, uint16_t len) 
{
    return QCLOUD_RET_SUCCESS;
}

#endif  // BLE_QTCIOT_LLSYNC_STANDARD

#if BLE_QTCIOT_LLSYNC_GATEWAY

typedef enum {
    TYPE_LLSYNC_SUBDEV_CTRL = 0,
    TYPE_LLSYNC_GROUP_CTRL  = 1,
    TYPE_LLSYNC_SCENE_CTRL  = 2,
    TYPE_LLSYNC_UNKNOW,
} LLsyncSubdevCtrlType;

/**
 * @brief llsync data characteristic uuid : 0xffe5
 *
 * @param[in] in_buf llsync data buffer
 * @param[in] in_len llsync data buffer length
 * @return 0 for success
 */
static int ble_subdev_ctrl_msg_handle(const char *in_buf, int in_len)
{
    POINTER_SANITY_CHECK(in_buf, QCLOUD_ERR_INVAL);

    char    *p_data     = (char *)in_buf;
    int      p_data_len = in_len;
    uint16_t tmp_len    = 0;
    uint8_t  header_len = 0;
    int      rc         = QCLOUD_RET_SUCCESS;

    // E_DEV_MSG_SYNC_TIME, E_DEV_MSG_CONN_VALID, E_DEV_MSG_BIND_SUCC, E_DEV_MSG_UNBIND this 4 type
    // of message has more than one bytes data, it may cut to several slices, here need to merge them
    // together, other type message only has 1 byte data, not need merge.
    if ((in_len > 3) && BLE_QTCIOT_IS_SLICE_PACKAGE(in_buf[1])) {
        // Log_dump( "slice", p_data, p_data_len);
        header_len = ble_msg_type_header_len(in_buf[0]);
        rc         = ble_package_slice_data(in_buf[0], in_buf[1], header_len, in_buf, in_len);
        if (rc < 0) {
            return QCLOUD_ERR_FAILURE;
        } else if (rc == 0) {
            tmp_len = HTONS(sg_ble_slice_data.buf_len - header_len);
            memcpy(&sg_ble_slice_data.buf[1], &tmp_len, sizeof(tmp_len));
            p_data     = sg_ble_slice_data.buf;
            p_data_len = sg_ble_slice_data.buf_len;
        } else if (rc > 0) {
            return QCLOUD_RET_SUCCESS;
        }
    }
    switch (p_data[0]) {
        case TYPE_LLSYNC_SUBDEV_CTRL:
            rc = iot_llsync_subdev_down_data_handle((uint8_t *)p_data, p_data_len);
            break;
        case TYPE_LLSYNC_SCENE_CTRL:
            rc = iot_llsync_scene_down_data_handle((uint8_t *)p_data, p_data_len);
            break;
        case TYPE_LLSYNC_GROUP_CTRL:
            rc = iot_llsync_group_down_data_handle((uint8_t *)p_data, p_data_len);
            break;
        default:
            Log_w("unknow type : %d", p_data[0]);
            rc = QCLOUD_ERR_FAILURE;
            break;
    }
    return rc;
}
#else
int ble_subdev_ctrl_msg_handle(const char *buf, uint16_t len)
{
    return QCLOUD_RET_SUCCESS;
}
#endif  // BLE_QTCIOT_LLSYNC_GATEWAY

// -----------------------------------------------------------------------------
// llsync link device
// -----------------------------------------------------------------------------

static void _llsync_connect_callback(void *usr_data, void *addr, size_t addr_len)
{
    ble_connection_state_set(E_BLE_CONNECTED);
    if (sg_iot_ble_llsync.callback.connect_callback) {
        sg_iot_ble_llsync.callback.connect_callback(sg_iot_ble_llsync.usr_data);
    }
    return;
}
static void _llsync_disconnect_callback(void *usr_data, void *addr, size_t addr_len)
{
    llsync_mtu_update(ATT_MTU_TO_LLSYNC_MTU(ATT_DEFAULT_MTU));
    llsync_connection_state_set(E_LLSYNC_DISCONNECTED);
    ble_connection_state_set(E_BLE_DISCONNECTED);
#if BLE_QTCIOT_SUPPORT_OTA
    ble_ota_stop();
#endif  // BLE_QTCIOT_SUPPORT_OTA
    if (sg_iot_ble_llsync.callback.disconnect_callback) {
        sg_iot_ble_llsync.callback.disconnect_callback(sg_iot_ble_llsync.usr_data);
    }
    // restart ble adv
    iot_llsync_start_adv(30);
    return;
}

static void _llsync_recv_callback(void *usr_data, void *addr, size_t addr_len, uint8_t *data, size_t data_len)
{
    uint16_t char_uuid = *((uint16_t *)addr);
    switch (char_uuid) {
            // device info
        case BLE_LLSYNC_CHARACTERISTIC_UUID_DEVICE_INFO:
            ble_device_info_msg_handle((char *)data, data_len);
            break;
            // data
        case BLE_LLSYNC_CHARACTERISTIC_UUID_DATA:
            ble_lldata_msg_handle((char *)data, data_len);
            break;
            // ota
        case BLE_LLSYNC_CHARACTERISTIC_UUID_OTA:
            ble_ota_msg_handle((char *)data, data_len);
            break;
            // sub dev ctrl
        case BLE_LLSYNC_CHARACTERISTIC_UUID_SUBDEV_CTRL:
            ble_subdev_ctrl_msg_handle((char *)data, data_len);
            break;
        default:
            Log_e("unknow characteristic 0x%04x", char_uuid);
            break;
    }
    return;
}

void _llsync_sync_mtu_callback(void *usr_data, uint16_t mtu)
{
    Log_i("ATT MTU : %d", mtu);
    llsync_mtu_update(ATT_MTU_TO_LLSYNC_MTU(mtu));
}

/**
 * @brief check iot llsync init
 *
 * @return IotBool
 */
IotBool iot_llsync_is_init(void)
{
    return sg_iot_ble_llsync.init_flag;
}

/**
 * @brief init llsync
 *
 * @param callback callback to user
 * @param usr_data
 * @return 0 for success
 */
int iot_llsync_init(TCIOTBLELLsyncInitParams init_params)
{
    if (sg_iot_ble_llsync.init_flag) {
        return QCLOUD_RET_SUCCESS;
    }
    // init
    memset(&sg_iot_ble_llsync, 0, sizeof(TCIOTBLELLsync));
    int               rc = 0;
    uint8_t           mac[6];
    IotLinkInitParams params;
    IotLinkNetwork    ble               = TCIOT_LINK_NETWORK_BLE;
    sg_iot_ble_llsync.usr_data          = init_params.usr_data;
    sg_iot_ble_llsync.callback          = init_params.callback;
    sg_iot_ble_llsync.wifi_usr_data     = NULL;
    params.callback.connect_callback    = _llsync_connect_callback;
    params.callback.disconnect_callback = _llsync_disconnect_callback;
    params.callback.recv_callback       = _llsync_recv_callback;
    params.callback.sync_mtu_callback   = _llsync_sync_mtu_callback;
    params.network                      = ble;
    params.usr_data                     = NULL;
    sg_iot_ble_llsync.link              = qcloud_iot_link_create(&params) ;
    rc                                  = qcloud_iot_link_get_uuid(sg_iot_ble_llsync.link, NULL, 0, mac, 6);
    if (rc) {
        Log_e("get mac fail.");
        goto exit;
    }
    rc = llsync_data_init(mac, init_params.dev_info);
    if (rc) {
        Log_e("llsync data init fail.");
        goto exit;
    }
    sg_iot_ble_llsync.init_flag = TCIOT_BOOL_TRUE;
    return iot_llsync_start_adv(init_params.start_adv);
exit:
    qcloud_iot_link_destroy(sg_iot_ble_llsync.link);
    sg_iot_ble_llsync.link = NULL;
    return rc;
}

/**
 * @brief llsync data send
 *
 * @param[in] char_uuid characteristic uuid
 * @param[in] data send data buffer
 * @param[in] data_len send data length
 * @return 0 for success
 */
int iot_llsync_send(uint16_t char_uuid, uint8_t *data, size_t data_len)
{
    return qcloud_iot_link_send(sg_iot_ble_llsync.link, &char_uuid, 2, data, data_len, 1000);
}

/**
 * @brief start ble advertising
 *
 * @return 0 for success
 */
int iot_llsync_start_adv(uint32_t adv_time)
{
    int     rc = 0;
    uint8_t adv_data[32];
    rc = iot_llsync_creat_adv_data(adv_data);
    if (rc < 0) {
        Log_e("creat adv data fail.rc = %d", rc);
        return rc;
    }
    return qcloud_iot_link_start_advertising(sg_iot_ble_llsync.link, adv_data, rc, adv_time);
}

/**
 * @brief stop ble advertising
 *
 * @return 0 for success
 */
int iot_llsync_stop_adv(void)
{
    return qcloud_iot_link_stop_advertising(sg_iot_ble_llsync.link);
}

/**
 * @brief register wifi callback function
 *
 * @param callback wifi callback function
 * @param usr_data
 */
void iot_llsync_register_wifi_callback(TCIOTBLELLsyncWifiCallback callback, void *usr_data)
{
    sg_iot_ble_llsync.wifi_callback = callback;
    sg_iot_ble_llsync.wifi_usr_data = usr_data;
}

/**
 * @brief unregister wifi callback function
 *
 */
void iot_llsync_unregister_wifi_callback(void)
{
    memset(&sg_iot_ble_llsync.wifi_callback, 0, sizeof(TCIOTBLELLsyncWifiCallback));
    sg_iot_ble_llsync.wifi_usr_data = NULL;
}
