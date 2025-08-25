/*****************************************************************************
 * Copyright (C) 2022 Tencent. All rights reserved.
 *
 * Licensed under the MIT License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "tc_iot_hal.h"
#include "utils_log.h"

#ifdef JIELI_RTOS
#include "fs/fs.h"
#endif

#ifdef JIELI_791N_RTOS
// 杰里ac79 长文件名必须转一下才能读出来
extern int long_file_name_encode(const char *input, u8 *output, u32 output_buf_len);
#endif

uint32_t HAL_FileGetDiskSize(void)
{
    // !获取文件系统剩余空间
    Log_e("HAL_FileGetDiskSize NOT SUPPORT YET");
    return 0;
}

void *HAL_FileOpen(const char *filename, const char *mode)
{
    if (filename == NULL || strlen(filename) == 0) {
        Log_e("invalid filename");
        return NULL;
    }

    if (mode == NULL || strlen(mode) == 0) {
        Log_e("invalid mode");
        return NULL;
    }
#ifdef JIELI_791N_RTOS
    uint8_t long_name[256] = {0};
    long_file_name_encode(filename, long_name, sizeof(long_name));
    return (void *)fopen((const char *)long_name, mode);
#else
    return (void *)fopen(filename, mode);
#endif
}

size_t HAL_FileRead(void *ptr, size_t size, size_t nmemb, void *fp)
{
    if (!ptr || !fp || 0 == (size*nmemb))  {
        Log_e("invalid params");
        return 0;
    }

#ifdef JIELI_RTOS
    // 杰理AC79的fread/fwrite返回值跟其他系统不同，这里调换nmemb和size来保持一致
#ifndef JIELI_5713_RTOS
    return fread(ptr, nmemb, size, (FILE *)fp);
#else
    return fread((FILE *)fp, ptr, size*nmemb);
#endif

#else
    return fread(ptr, size, nmemb, (FILE *)fp);
#endif
}

size_t HAL_FileWrite(const void *ptr, size_t size, size_t nmemb, void *fp)
{
    if (!ptr || !fp || 0 == (size * nmemb)) {
        Log_e("ptr : %p fp : %p size : %d nmemb : %d\n", ptr, fp, size, nmemb);
        Log_e("invalid params");
        return 0;
    }
#ifdef JIELI_RTOS
    // 杰理AC79的fread/fwrite返回值跟其他系统不同，这里调换nmemb和size来保持一致
#ifndef JIELI_5713_RTOS
    return fwrite(ptr, nmemb, size, (FILE *)fp);
#else
    return fwrite((FILE *)fp, ptr, size*nmemb);
#endif
#else
    return fwrite(ptr, size, nmemb, (FILE *)fp);
#endif
}

int HAL_FileSeek(void *fp, long int offset, int wh)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

    int whence = SEEK_SET;
    switch(wh) {
        case HAL_SEEK_SET:
            whence = SEEK_SET;
            break;
        case HAL_SEEK_CUR:
            whence = SEEK_CUR;
            break;
        case HAL_SEEK_END:
            whence = SEEK_END;
            break;
        default:
            break;
    }

    return fseek((FILE *)fp, offset, whence);
}

int HAL_FileClose(void *fp)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

    return fclose((FILE *)fp);
}

int HAL_FileRemove(const char *filename)
{
    if (filename == NULL || strlen(filename) == 0) {
        Log_e("invalid filename");
        return -1;
    }

#ifdef JIELI_RTOS
    return fdelete_by_name(filename);
#else
    return remove(filename);
#endif
}

int HAL_FileRewind(void *fp)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

    // rewind will clear the error indicator
#ifdef JIELI_RTOS
    fseek((FILE *)fp, 0, SEEK_SET);
#else
    rewind((FILE *)fp);
#endif
    return 0;
}

long HAL_FileTell(void *fp)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

    return ftell((FILE *)fp);
}

long HAL_FileSize(void *fp)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

    FILE *f = (FILE *)fp;
    long size = 0;
#ifdef JIELI_RTOS
    size = flen(fp);
#else
    size_t tmp_pos = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, tmp_pos, SEEK_SET);
#endif

    return size;
}

char *HAL_FileGets(char *str, int n, void *fp)
{
    if (!str || !fp || !n)  {
        Log_e("invalid params");
        return NULL;
    }
    // fgets return when read eof or end of line

#ifdef JIELI_RTOS
#ifndef JIELI_5713_RTOS
    fread((void *)str, n, 1, (FILE *)fp);
#else
    fread((FILE *)fp, (void *)str, n);
#endif
    return str;
#else
    return fgets(str, n, (FILE *)fp);
#endif
}

int HAL_FileRename(const char *old_filename, const char *new_filename)
{
    if (!old_filename || !new_filename)  {
        Log_e("invalid params");
        return 0;
    }

#ifdef JIELI_RTOS
    return -1;
#else
    return rename(old_filename, new_filename);
#endif
}

int HAL_FileEof(void *fp)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

#ifdef JIELI_RTOS
    FILE *f = (FILE *)fp;
    size_t tmp_pos = ftell(f);
    fseek(f, 0, SEEK_END);
    size_t tmp_file_size = flen(f);
    fseek(f, tmp_pos, SEEK_SET);
    if (tmp_pos == tmp_file_size) {
        return 1;
    }

    return 0;
#else
    return feof((FILE *)fp);
#endif
}

int HAL_FileError(void *fp)
{
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

#ifdef JIELI_RTOS
    return -1;
#else
    return ferror((FILE *)fp);
#endif
}

int HAL_FileFlush(void *fp)
{
#ifndef JIELI_RTOS
    if (!fp) {
        Log_e("invalid file handle");
        return -1;
    }

    return fflush((FILE *)fp);
#else
    return 0;
#endif
}
