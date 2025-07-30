/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Function pointer type for registering console commands
 *         This callback type is used for registering custom console commands
 *         The implementation should use esp_console_cmd_register() to add commands
 *
 * @note  The registered commands will be available in the console after registration
 *
 * Example:
 * @code{c}
 *           void register_my_commands(void) {
 *               const esp_console_cmd_t cmd = {
 *                   .command = "mycmd",
 *                   .help = "My custom command",
 *                   .func = &my_command_handler
 *               };
 *               esp_console_cmd_register(&cmd);
 *           }
 *
 * @endcode
 */
typedef void (*console_cmds_register)(void);

/**
 * @brief  Initialize the Command Line Interface (CLI) with custom prompt and commands
 *         This function initializes the CLI system with a custom prompt and registers user-defined commands
 *         It sets up the console environment and prepares it for command input and processing
 *
 * @param[in]  prompt  The command prompt string to display (e.g. "esp32>")
 * @param[in]  cmds    Callback function to register custom CLI commands
 *
 * @return
 *       - ESP_OK               Success, CLI initialized successfully or already initialized
 *       - ESP_ERR_INVALID_ARG  Invalid prompt or callback
 *       - ESP_ERR_NO_MEM       Failed to allocate memory for CLI
 *       - ESP_FAIL             CLI initialization failed
 *
 * @note  The function logs initialization status:
 *        - "CLI already initialized" when called multiple times
 *        - "CLI initialized successfully" on first successful initialization
 *        - Error details on initialization failure
 *
 *Example:
 * @code{c}
 *               // Command registration function
 *           void register_app_commands(void) {
 *               // Register your commands here
 *           }
 *
 *           // Initialize CLI
 *           esp_err_t ret = esp_gmf_app_cli_init("gmf> ", register_app_commands);
 *           if (ret != ESP_OK) {
 *               // Handle error
 *           }
 *
 * @endcode
 */
esp_err_t esp_gmf_app_cli_init(const char *prompt, console_cmds_register cmds);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
