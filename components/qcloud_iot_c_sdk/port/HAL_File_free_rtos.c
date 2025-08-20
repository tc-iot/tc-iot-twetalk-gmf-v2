/**
 * @file HAL_File_Linux.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-01-11
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
 * @par Change File:
 * <table>
 * Date				Version		Author			Description
 * 2022-01-11		1.0			hubertxxu		first commit
 * </table>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "qcloud_iot_platform.h"

/**
 * @brief Functions for saving file into NVS(files/FLASH)
 * @param[in] filename file path name
 * @param[in] buf source need write buffer
 * @param[in] write_len length of file to write
 * @return length of data save when success, or 0 for failure
 */
size_t HAL_File_Write(const char *filename, const void *buf, size_t write_len, size_t offset)
{
    int    rc  = -1;
    size_t len = 0;

    FILE *fp = fopen(filename, "rb+");
    if (!fp) {
        fp = fopen(filename, "wb+");
        if (!fp) {
            Log_e("create file %s failed", filename);
            return 0;
        }
    }

    rc = fseek(fp, offset, SEEK_SET);
    if (!rc) {
        len = fwrite(buf, 1, write_len, fp);
    }
    fclose(fp);
    return len;
}

/**
 * @brief Functions for reading file from NVS(files/FLASH)
 * @param[in] filename file path name
 * @param[in] buf destination log buffer
 * @param[in] read_len length to read
 * @return length of data read when success, or 0 for failure
 */
size_t HAL_File_Read(const char *filename, void *buf, size_t read_len, size_t offset)
{
    int    rc  = 0;
    size_t len = 0;

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return 0;
    }

    rc = fseek(fp, offset, SEEK_SET);
    if (!rc) {
        len = fread(buf, 1, read_len, fp);
    }
    fclose(fp);
    return len;
}

/**
 * @brief Functions for deleting file in NVS(files/FLASH).
 * @param[in] filename file path name
 * @return 0 when success
 */
int HAL_File_Del(const char *filename)
{
    return remove(filename);
}

/**
 * @brief Functions for reading the size of file in NVS(files/FLASH).
 * @param[in] filename file path name
 * @return 0 when nothing exist
 */
size_t HAL_File_GetSize(const char *filename)
{
    long  length;
    FILE *fp;

    /* check if file exists */
    if (access(filename, 0)) {
        return 0;
    }

    if ((fp = fopen(filename, "r")) == NULL) {
        Log_e("fail to open file %s", filename);
        return 0;
    }

    fseek(fp, 0L, SEEK_END);
    length = ftell(fp);
    fclose(fp);

    return length > 0 ? (size_t)length : 0;
}
