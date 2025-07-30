/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Start monitor of app system
 */
void esp_gmf_app_sys_monitor_start(void);

/**
 * @brief  Stop monitor of app system
 */
void esp_gmf_app_sys_monitor_stop(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
