/**
 * @file http_signed.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-01-21
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
 * 2022-01-21		1.0			hubertxxu		first commit
 * </table>
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qcloud_iot_http_signed.h"

#include "utils_hmac.h"
#include "utils_base64.h"

/**
 * @brief http signed request header.
 *
 */
#define HTTP_SIGNED_REQUEST_HEADER_LEN (1024)

/**
 * @brief http signed request header signed length
 *
 */
#define HTTP_SIGNED_STRING_BUFFER_LEN (256)

/**
 * @brief http signed request url length
 *
 */
#define HTTP_SIGNED_URL_LEN (128)

#define QCLOUD_SHA256_RESULT_LEN (32)
#define QCLOUD_SHA1_RESULT_LEN   (20)

typedef struct {
    HttpSignedParams     params;
    void                *http_client;
    IotHTTPRequestParams http_request;
    char                *sign_string;
    char                *url;
} HTTPSignedHandle;

/**
 * @brief Connect http server.
 *
 * @param[in,out] handle pointer to http signed handle, @see HTTPSignedHandle
 * @return 0 for success. others @see IotReturnCode
 */
static int _http_signed_connect(HTTPSignedHandle *handle)
{
    IotHTTPConnectParams connect_params = {
        .url    = handle->url,
        .port   = "80",
        .ca_crt = NULL,  // TODO: support cert

    };
    return TCIOT_HTTP_Connect(handle->http_client, &connect_params);
}

/**
 * @brief Generate header.
 *
 * @param handle
 * @param timestamp
 * @param nonce
 * @param sign
 */
static void _http_signed_upload_header_construct(HTTPSignedHandle *handle, uint32_t timestamp, int nonce,
                                                 const char *sign)
{
/**
 * @brief http signed header format
 *
 */
#define QCLOUD_HTTP_HEADER_FORMAT      \
    "Accept: application/json;*/*\r\n" \
    "X-TC-Algorithm: hmacsha1\r\n"     \
    "X-TC-Timestamp: %d\r\n"           \
    "X-TC-Nonce: %d\r\n"               \
    "X-TC-Signature: %s\r\n"
    TCI_HAL_Snprintf(handle->http_request.header, HTTP_SIGNED_REQUEST_HEADER_LEN, QCLOUD_HTTP_HEADER_FORMAT, timestamp,
                 nonce, sign);
    // Log_d("header:%s", handle->http_request.header);
}

/**
 * @brief http signed header construct
 *
 * @param[in,out] handle pointer to http signed handle, @see HTTPSignedHandle
 * @param request_body_buf
 * @param request_body_buf_len
 * @return 0 if success
 */
static void _http_signed_upload_header_new(HTTPSignedHandle *handle, const char *request_body_buf,
                                           int request_body_buf_len)
{
    uint8_t  sign[QCLOUD_SHA1_RESULT_LEN]                     = {0};
    char     sign_out[QCLOUD_SHA1_RESULT_LEN * 2]             = {0};
    char     request_buf_sha1[QCLOUD_SHA1_RESULT_LEN * 2 + 1] = {0};
    size_t   olen                                             = 0;
    int      nonce                                            = TCI_HAL_Random();
    uint32_t timestamp                                        = TCI_HAL_GetTimeSecond();

    memset(handle->sign_string, 0, HTTP_SIGNED_STRING_BUFFER_LEN);
    /* cal hmac sha1 */
    utils_sha1_hex((const uint8_t *)request_body_buf, request_body_buf_len, (uint8_t *)request_buf_sha1);
    /* create sign string */
    TCI_HAL_Snprintf(handle->sign_string, HTTP_SIGNED_STRING_BUFFER_LEN, "%s\n%s\n\nhmacsha1\n%s\n%d\n%d\n%s", "POST",
                 handle->params.host, handle->params.uri, timestamp, nonce, request_buf_sha1);
    utils_hmac_sha1((const uint8_t *)handle->sign_string, strlen(handle->sign_string),
                    (uint8_t *)handle->params.secret_key, strlen(handle->params.secret_key), sign);
    /* base64 encode */
    utils_base64encode(sign_out, QCLOUD_SHA1_RESULT_LEN * 2, &olen, sign, QCLOUD_SHA1_RESULT_LEN);
    _http_signed_upload_header_construct(handle, timestamp, nonce, sign_out);
}

/**
 * @brief http signed init.
 *
 * @param params @see HttpSignedParams
 * @return pointer to http signed @see HTTPSignedHandle
 */
void *_http_signed_init(HttpSignedParams *params)
{
    HTTPSignedHandle *handle = TCI_HAL_Malloc(sizeof(HTTPSignedHandle));
    if (!handle) {
        goto exit;
    }

    handle->http_client = TCIOT_HTTP_Init();
    if (!handle->http_client) {
        goto exit;
    }

    handle->http_request.header = TCI_HAL_Malloc(HTTP_SIGNED_REQUEST_HEADER_LEN);
    if (!handle->http_request.header) {
        goto exit;
    }
    memset(handle->http_request.header, 0, HTTP_SIGNED_REQUEST_HEADER_LEN);

    handle->sign_string = TCI_HAL_Malloc(HTTP_SIGNED_STRING_BUFFER_LEN);
    if (!handle->sign_string) {
        goto exit;
    }
    memset(handle->sign_string, 0, HTTP_SIGNED_STRING_BUFFER_LEN);

    handle->url = TCI_HAL_Malloc(HTTP_SIGNED_URL_LEN);
    if (!handle->url) {
        goto exit;
    }

    memcpy(&handle->params, params, sizeof(HttpSignedParams));
    memset(handle->url, 0, HTTP_SIGNED_URL_LEN);
    TCI_HAL_Snprintf(handle->url, HTTP_SIGNED_URL_LEN, "%s://%s%s", "http", handle->params.host, handle->params.uri);

    return handle;
exit:
    if (handle) {
        TCI_HAL_Free(handle->http_request.header);
        TCI_HAL_Free(handle->sign_string);
        TCI_HAL_Free(handle->url);
        TCIOT_HTTP_Deinit(handle->http_client);
        TCI_HAL_Free(handle);
    }
    return NULL;
}

/**
 * @brief send to server
 *
 * @param[in] handle pointer to http signed @see HTTPSignedHandle
 * @param[in] upload_buf need send data
 * @param[in] upload_len send length
 * @return int 0 for success. others @see IotReturnCode
 */
int _http_signed_upload(HTTPSignedHandle *handle, const char *upload_buf, size_t upload_len)
{
    int rc = 0;

    rc = _http_signed_connect(handle);
    if (rc) {
        return rc;
    }
    if (!handle->params.sign) {
        _http_signed_upload_header_new(handle, upload_buf, upload_len);
    } else {
        _http_signed_upload_header_construct(handle, handle->params.time_stamp, handle->params.nonce,
                                             handle->params.sign);
    }

    handle->http_request.url            = handle->url;
    handle->http_request.method         = TCIOT_HTTP_METHOD_POST;
    handle->http_request.content_type   = "application/json;charset=utf-8";
    handle->http_request.content        = (char *)upload_buf;
    handle->http_request.content_length = upload_len;

    rc = TCIOT_HTTP_Request(handle->http_client, &handle->http_request);
    return rc;
}

/**
 * @brief http signed deinit
 *
 * @param[in] handle pointer to http signed @see HTTPSignedHandle
 */
static void _http_signed_deinit(HTTPSignedHandle *signed_handle)
{
    POINTER_SANITY_CHECK_RTN(signed_handle);
    TCIOT_HTTP_Disconnect(signed_handle->http_client);
    TCIOT_HTTP_Deinit(signed_handle->http_client);
    TCI_HAL_Free(signed_handle->http_request.header);
    TCI_HAL_Free(signed_handle->sign_string);
    TCI_HAL_Free(signed_handle->url);
    TCI_HAL_Free(signed_handle);
}

/**
 * @brief post message and recv response
 *
 * @param params @see HttpSignedParams
 * @param request_buf   request buffer
 * @param request_buf_len request buffer length
 * @param response_buf   response buffer if need recv
 * @param response_buf_len response buffer length if need recv
 * @return int if need recv return recv length else return 0 is success
 */
int TCIOT_HTTP_SignedRequest(HttpSignedParams *params, const char *request_buf, size_t request_buf_len,
                           uint8_t *response_buf, int response_buf_len)
{
    int rc = 0;
    POINTER_SANITY_CHECK(params, QCLOUD_ERR_INVAL);
    POINTER_SANITY_CHECK(request_buf, QCLOUD_ERR_INVAL);
    if (params->need_recv) {
        POINTER_SANITY_CHECK(response_buf, QCLOUD_ERR_INVAL);
    }
    HTTPSignedHandle *handle = _http_signed_init(params);
    if (!handle) {
        Log_e("http signed init error.");
        goto exit;
    }
    rc = _http_signed_upload(handle, request_buf, request_buf_len);
    if (rc) {
        Log_e("http signed upload error.");
        goto exit;
    }
    if (params->need_recv) {
        rc = TCIOT_HTTP_Recv(handle->http_client, response_buf, response_buf_len, params->recv_timeout_ms);
    }

exit:
    _http_signed_deinit(handle);
    return rc;
}
