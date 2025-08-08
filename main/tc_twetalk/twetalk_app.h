/**
 * @file twetalk_app.h
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-31
 * 
 * @copyright
 * Tencent is pleased to support the open source community by making IoT Hub available. 
 * Copyright(C) 2021 - 2026 THL A29 Limited, a Tencent company.All rights reserved.
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

 # pragma once

 typedef enum {
    LOCALPLAY_CONNECTING = 0, // 正在联网
    LOCALPLAY_CONNECTED, // 联网成功
    LOCALPLAY_HELLO, // 收到hello
    LOCALPLAY_DONG, // 收到dong
    LOCALPLAY_PAIR_NETWORK, // 配网
    LOCALPLAY_CLEAR_NETWORK, // 清除配网
    LOCALPLAY_ENTER_KEY_MODE, // 进入按键模式
    LOCALPLAY_EXIT_KEY_MODE, // 退出按键模式
    LOCALPLAY_MAX,
}LocalPlayE;
extern const char* tone_uri[];

int tc_twetalk_init(void);