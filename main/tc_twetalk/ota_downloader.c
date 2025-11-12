/**
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
 * @file ota_downloader.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-10-20
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-10-20 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "ota_downloader.h"

#include "esp_gmf_oal_thread.h"
#include "nvs_flash.h"
#include "qcloud_iot_ota.h"
#include "utils_downloader.h"
#include "utils_md5.h"

#include "esp_log.h"

#define TAG "ota"

/**
 * @brief Resource info.
 *
 */
typedef struct {
    char version[MAX_SIZE_OF_FW_VERSION + 1];
    uint32_t file_size;
    char md5sum[33];
} ResourceInfo;

/**
 * @brief Break point info.
 *
 */
typedef struct {
    ResourceInfo file_id;
    uint32_t downloaded_size;
} ResourceDownloadInfo;

/**
 * @brief OTA downloader handle.
 *
 */
typedef struct {
    // no change
    void* cos_download;
    void* mqtt_client;
    void* downloader;
    int (*on_download_finish)(const char* filename, size_t total_len);
    const char* (*get_firmware_version)(void);
    uint8_t is_ota_process_exit;
    void* resource_sem;
    void* resource_mutex;
    // change
    ResourceInfo download_now;
    ResourceDownloadInfo break_point;
    char download_url[MAX_SIZE_OF_DOWNLOAD_URL];
    uint8_t download_buff[RESOURCE_HTTP_BUF_SIZE];
    int download_size;
    int download_percent;
    IotMd5Context download_md5_ctx;
    UtilsDownloaderStatus download_status;
} ResourceDownloaderHandle;

/**
 * @brief Handle.
 *
 */
static ResourceDownloaderHandle sg_resource_downloader_handle = {0};

//  ----------------------------------------------------------------------------
//  read/write break point function
//  TODO : 根据当前的硬件配置，自行选择存储方式，如SPIFFS、NVS、SDCARD等
//  ----------------------------------------------------------------------------

static int _read_break_point(ResourceDownloadInfo* handle)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    size_t length;

    // open
    err = nvs_open("ota_break_info", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    }

    // read break point data
    length = sizeof(ResourceDownloadInfo);
    err    = nvs_get_blob(my_handle, "break_point", handle, &length);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error (%s) reading break point from NVS!\n", esp_err_to_name(err));
    }

    nvs_close(my_handle);
    return err;
}

static int _write_break_point(ResourceDownloaderHandle* handle)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // open
    err = nvs_open("ota_break_info", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    // write break point data
    err = nvs_set_blob(my_handle, "break_point", &handle->break_point, sizeof(handle->break_point));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) writing break point to NVS!", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    // commit changes
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS changes!", esp_err_to_name(err));
    }

    nvs_close(my_handle);
    return err;
}

// ----------------------------------------------------------------------------
// download finish function
// ----------------------------------------------------------------------------

static void _report_download_result(ResourceDownloaderHandle* handle, UtilsDownloaderStatus status, int result)
{
    char buf[256];
    int buf_len = sizeof(buf);
    switch (status) {
        case UTILS_DOWNLOADER_STATUS_SUCCESS:
            TCIOT_OTA_ReportProgress(handle->mqtt_client, buf, buf_len,
                                   result ? TCIOT_OTA_REPORT_TYPE_UPGRADE_SUCCESS : TCIOT_OTA_REPORT_TYPE_MD5_NOT_MATCH, 0,
                                   handle->download_now.version);
            break;
        case UTILS_DOWNLOADER_STATUS_NETWORK_FAILED:
            TCIOT_OTA_ReportProgress(handle->mqtt_client, buf, buf_len, TCIOT_OTA_REPORT_TYPE_DOWNLOAD_TIMEOUT, 0,
                                   handle->download_now.version);
            break;
        case UTILS_DOWNLOADER_STATUS_BREAK_POINT_FAILED:
        case UTILS_DOWNLOADER_STATUS_DATA_DOWNLOAD_FAILED:
            TCIOT_OTA_ReportProgress(handle->mqtt_client, buf, buf_len, TCIOT_OTA_REPORT_TYPE_UPGRADE_FAIL, 0,
                                   handle->download_now.version);
            break;
        default:
            break;
    }
}

// ----------------------------------------------------------------------------
// Downloader function
// ----------------------------------------------------------------------------

// break point function

/**
 * @brief Read break point from NVS.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_break_point_init(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;

    sg_resource_downloader_handle.cos_download = NULL;
    utils_md5_reset(&handle->download_md5_ctx);

    // Try to read break point from NVS
    int ret = _read_break_point(&handle->break_point);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        Log_w("read break point from NVS fail!");
        return -1;
    }

    return 0;
}

/**
 * @brief Memset break point.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 */
static void _resource_break_point_deinit(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    memset(&handle->break_point, 0, sizeof(handle->break_point));
}

/**
 * @brief Set break point using download now info.
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_break_point_set(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    // read break point info from flash
    memset(&handle->break_point, 0, sizeof(handle->break_point));
    memcpy(&handle->break_point.file_id, &handle->download_now, sizeof(handle->break_point.file_id));
    return 0;
}

/**
 * @brief Update break point and save.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_break_point_save(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;

    if (handle->download_size <= 0) {
        return -1;
    }

    // update md5 sum & download_size
    handle->break_point.downloaded_size += handle->download_size;
    utils_md5_update(&handle->download_md5_ctx, handle->download_buff, handle->download_size);

    // report progress
    char buf[256];
    int buf_len = sizeof(buf);

    int percent = handle->break_point.downloaded_size * 100 / handle->download_now.file_size;
    if (percent > 100) {
        return -1;
    }

    if (handle->download_percent != percent && (percent - handle->download_percent >= 10)) {
        TCIOT_OTA_ReportProgress(handle->mqtt_client, buf, buf_len, TCIOT_OTA_REPORT_TYPE_DOWNLOADING, percent,
                               handle->break_point.file_id.version);
        handle->download_percent = percent;
        Log_i("downloading %d%%...", percent);
    }

    // write to NVS
    return _write_break_point(handle);
}

/**
 * @brief Check if break point matches download now info.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_break_point_check(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    // version should be the same, download size should not bigger than file size
    Log_d("download now:%s,%d,%s", handle->download_now.version, handle->download_now.file_size,
          handle->download_now.md5sum);
    Log_d("break point:%s,%d,%d,%s", handle->break_point.file_id.version, handle->break_point.file_id.file_size,
          handle->break_point.downloaded_size, handle->break_point.file_id.md5sum);
    return strncmp(handle->break_point.file_id.version, handle->download_now.version, MAX_SIZE_OF_FW_VERSION) ||
           handle->break_point.file_id.file_size != handle->download_now.file_size ||
           handle->break_point.downloaded_size > handle->download_now.file_size ||
           strncmp(handle->break_point.file_id.md5sum, handle->download_now.md5sum, 32);
}

/**
 * @brief Calculate md5 sum according break point.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_break_point_restore(void* usr_data)
{
    int rc                           = 0;
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;

    // update md5 according downloaded data
    size_t rlen, total_read = 0, size = 0;

    size = handle->break_point.downloaded_size;

    while (size > 0) {
        rlen = (size > RESOURCE_HTTP_BUF_SIZE) ? RESOURCE_HTTP_BUF_SIZE : size;

        rc = TCI_HAL_OTA_read_flash(handle, total_read, handle->download_buff, rlen);
        if (rc) {
            Log_e("read data failed rc : %d", rc);
            handle->break_point.downloaded_size = 0;
            break;
        }
        utils_md5_update(&handle->download_md5_ctx, handle->download_buff, rlen);
        size -= rlen;
        total_read += rlen;
    }
    return 0;
}

// data download function

/**
 * @brief Init cos downloader.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_data_download_init(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    IotCosDownloadParams params      = {
             .url              = handle->download_url,
             .offset           = handle->break_point.downloaded_size,
             .file_size        = handle->break_point.file_id.file_size,
             .is_fragmentation = TCIOT_BOOL_FALSE,
             .is_https_enabled = TCIOT_BOOL_FALSE,
    };
    handle->cos_download = TCIOT_COS_DownloadInit(&params);
    return handle->cos_download ? 0 : -1;
}

/**
 * @brief Deinit cos downloader.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 */
static void _resource_data_download_deinit(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    TCIOT_COS_DownloadDeinit(handle->cos_download);
}

/**
 * @brief Check if download finished.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_data_download_is_over(void* usr_data)
{
    // check download is over
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    return handle->break_point.downloaded_size == handle->download_now.file_size;
}

/**
 * @brief Download from cos server.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_data_download_recv(void* usr_data)
{
    // download data using http
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    // TODO: https download
    handle->download_size = TCIOT_COS_DownloadFetch(handle->cos_download, handle->download_buff, RESOURCE_HTTP_BUF_SIZE,
                                                  RESOURCE_HTTP_TIMEOUT_MS);
    return handle->download_size > 0 ? 0 : -1;
}

/**
 * @brief Sava firmware to file.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @return 0 for success
 */
static int _resource_data_download_save(void* usr_data)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;
    return handle->download_size > 0 ? TCI_HAL_OTA_write_flash(usr_data, handle->break_point.downloaded_size,
                                                           handle->download_buff, handle->download_size)
                                     : 0;
}

/**
 * @brief Process download result.
 *
 * @param[in,out] usr_data @see ResourceDownloaderHandle
 * @param[in] status status when finish download, @see UtilsDownloaderStatus
 * @return 0 for success
 */
static int _resource_data_download_finish(void* usr_data, UtilsDownloaderStatus status)
{
    ResourceDownloaderHandle* handle = (ResourceDownloaderHandle*)usr_data;

    int rc = 0, valid = 0;
    switch (status) {
        case UTILS_DOWNLOADER_STATUS_SUCCESS:
            utils_md5_finish(&handle->download_md5_ctx);
            valid = !utils_md5_compare(&handle->download_md5_ctx, handle->download_now.md5sum);
            if (valid) {  // if valid, then call user ext function
                rc = sg_resource_downloader_handle.on_download_finish(handle->download_now.version,
                                                                      handle->download_now.file_size);
            }
            // reset break point
            if (valid || !rc) {
                memset(&handle->break_point, 0, sizeof(handle->break_point));
                _write_break_point(handle);
            }
            // report result
            _report_download_result(handle, status, valid && !rc);
            break;
        case UTILS_DOWNLOADER_STATUS_NETWORK_FAILED:
        case UTILS_DOWNLOADER_STATUS_BREAK_POINT_FAILED:
        case UTILS_DOWNLOADER_STATUS_DATA_DOWNLOAD_FAILED:
            _report_download_result(handle, status, 0);
            break;
        default:
            break;
    }
    handle->download_status = UTILS_DOWNLOADER_STATUS_SUCCESS;
    return !(valid && !rc);
}

// ----------------------------------------------------------------------------
// API
// ----------------------------------------------------------------------------

/**
 * @brief Init ota downloader.
 *
 * @param[in,out] client pointer to mqtt client
 * @return 0 for success.
 */
int _resource_downloader_init(void* client)
{
    // downloader init
    UtilsDownloaderFunction ota_callback = {
        .downloader_malloc = TCI_HAL_Malloc,
        .downloader_free   = TCI_HAL_Free,
        // break point
        .break_point_init    = _resource_break_point_init,
        .break_point_deinit  = _resource_break_point_deinit,
        .break_point_set     = _resource_break_point_set,
        .break_point_save    = _resource_break_point_save,
        .break_point_check   = _resource_break_point_check,
        .break_point_restore = _resource_break_point_restore,

        // data download
        .data_download_init    = _resource_data_download_init,
        .data_download_deinit  = _resource_data_download_deinit,
        .data_download_is_over = _resource_data_download_is_over,
        .data_download_recv    = _resource_data_download_recv,
        .data_download_save    = _resource_data_download_save,
        .data_download_finish  = _resource_data_download_finish,
    };

    sg_resource_downloader_handle.downloader = utils_downloader_init(ota_callback, &sg_resource_downloader_handle);
    if (!sg_resource_downloader_handle.downloader) {
        Log_e("initialize downloaded failed");
        return QCLOUD_ERR_MALLOC;
    }
    sg_resource_downloader_handle.mqtt_client     = client;
    sg_resource_downloader_handle.download_status = UTILS_DOWNLOADER_STATUS_INIT;
    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief Set download info of ota firmware.
 *
 * @param[in] firmware_info pointer to firmware info
 * @param[in] url url of cos download
 * @param[in] url_len download length
 */
void _resource_downloader_info_set(ResourceInfo* resource_info, const char* url, int url_len)
{
    if (UTILS_DOWNLOADER_STATUS_DOWNLOADING != sg_resource_downloader_handle.download_status) {
        memset(&sg_resource_downloader_handle.download_now, 0,
               (uintptr_t)(&sg_resource_downloader_handle.download_status) -
                   (uintptr_t)(&sg_resource_downloader_handle.download_now));
        memcpy(&sg_resource_downloader_handle.download_now, resource_info, sizeof(ResourceInfo));
        memcpy(sg_resource_downloader_handle.download_url, url, url_len);
        sg_resource_downloader_handle.download_status = UTILS_DOWNLOADER_STATUS_DOWNLOADING;
    }
}

/**
 * @brief Deinit ota downloader.
 *
 */
void _resource_downloader_deinit(void)
{
    utils_downloader_deinit(sg_resource_downloader_handle.downloader);
    memset(&sg_resource_downloader_handle, 0, sizeof(sg_resource_downloader_handle));
}

// ----------------------------------------------------------------------------
// ota function
// ----------------------------------------------------------------------------

static void _update_firmware_callback(UtilsJsonValue version, UtilsJsonValue url, UtilsJsonValue md5sum,
                                      uint32_t file_size, void* usr_data)
{
    Log_i("recv firmware: version=%.*s|url=%.*s|md5sum=%.*s|file_size=%u", version.value_len, version.value,
          url.value_len, url.value, md5sum.value_len, md5sum.value, file_size);
    // only one firmware one time is supportted now
    ResourceInfo resource_info;
    memset(&resource_info, 0, sizeof(ResourceInfo));
    strncpy(resource_info.version, version.value, version.value_len);
    strncpy(resource_info.md5sum, md5sum.value, md5sum.value_len);
    resource_info.file_size = file_size;
    _resource_downloader_info_set(&resource_info, url.value, url.value_len);
    TCI_HAL_SemaphorePost(sg_resource_downloader_handle.resource_sem);
}

int iot_ota_init(void* client, const IotOtaInitParams* params)
{
    int rc;

    rc = _resource_downloader_init(client);
    if (rc) {
        return rc;
    }

    if (!sg_resource_downloader_handle.resource_sem) {
        sg_resource_downloader_handle.resource_sem = TCI_HAL_SemaphoreCreate();
        if (!sg_resource_downloader_handle.resource_sem) {
            Log_e("resource sem create failed");
            return QCLOUD_ERR_MALLOC;
        }
    }
    if (!sg_resource_downloader_handle.resource_mutex) {
        sg_resource_downloader_handle.resource_mutex = TCI_HAL_MutexCreate();
        if (!sg_resource_downloader_handle.resource_mutex) {
            Log_e("resource task mutex failed");
            return QCLOUD_ERR_MALLOC;
        }
    }
    sg_resource_downloader_handle.on_download_finish   = params->on_download_finish;
    sg_resource_downloader_handle.get_firmware_version = params->get_firmware_version;

    IotOTAUpdateCallback ota_callback = {
        .update_firmware_callback      = _update_firmware_callback,
        .report_version_reply_callback = NULL,
    };

    rc = TCIOT_OTA_Init(client, ota_callback, NULL);
    if (rc) {
        Log_e("OTA init failed!, rc=%d", rc);
        return rc;
    }
    sg_resource_downloader_handle.is_ota_process_exit = 0;
    return iot_ota_report_version();
}

void iot_ota_process(void* param)
{
    TCI_HAL_MutexLock(sg_resource_downloader_handle.resource_mutex);

    while (!sg_resource_downloader_handle.is_ota_process_exit) {
        TCI_HAL_SemaphoreWait(sg_resource_downloader_handle.resource_sem, 0xffffffff);
        if (UTILS_DOWNLOADER_STATUS_DOWNLOADING == sg_resource_downloader_handle.download_status) {
            utils_downloader_process(sg_resource_downloader_handle.downloader);
            TCI_HAL_SleepMs(10);
        }
    }
    TCI_HAL_MutexUnlock(sg_resource_downloader_handle.resource_mutex);
    esp_gmf_oal_thread_delete(param);
}

int iot_ota_report_version(void)
{
    int rc        = 0;
    char buf[256] = {0};
    rc            = TCIOT_OTA_ReportVersion(sg_resource_downloader_handle.mqtt_client, buf, sizeof(buf),
                                          sg_resource_downloader_handle.get_firmware_version());
    return rc < 0;
}

void iot_ota_deinit(void)
{
    sg_resource_downloader_handle.is_ota_process_exit = 1;

    if (sg_resource_downloader_handle.resource_sem) {
        TCI_HAL_SemaphorePost(sg_resource_downloader_handle.resource_sem);
    }

    if (sg_resource_downloader_handle.resource_mutex) {
        TCI_HAL_MutexLock(sg_resource_downloader_handle.resource_mutex);
        TCI_HAL_MutexUnlock(sg_resource_downloader_handle.resource_mutex);
    }

    TCI_HAL_MutexDestroy(sg_resource_downloader_handle.resource_mutex);
    TCI_HAL_SemaphoreDestroy(sg_resource_downloader_handle.resource_sem);
    TCIOT_OTA_Deinit(sg_resource_downloader_handle.mqtt_client);
    _resource_downloader_deinit();
}
