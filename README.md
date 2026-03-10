# 腾讯云 TWeTalk 智能语音设备接入

基于 ESP-GMF 框架的智能语音交互设备解决方案，集成腾讯云 TWeTalk SDK，支持语音对话、音乐点播、微信电话等功能。

> **📖 前置阅读**：[TWeTalk 设备接入指引](https://doc.weixin.qq.com/doc/w3_AJkAsAbTADsCNhIeEPMZsTXCETJk6?scode=AJEAIQdfAAot612P7vAJkAsAbTADs)

## 技术特性

- **多种交互模式**：连续对话 / 语音唤醒（"Hi 乐鑫"） / 按键触发
- **专业音频处理**：集成 AEC/AGC/ANR 3A 算法
- **云端智能服务**：腾讯云 TWeTalk SDK，实时语音识别与合成
- **扩展功能**：音乐点播、微信电话、自然语言理解

**技术栈**：ESP-IDF v5.4+ / ESP-GMF / TWeTalk SDK / WebSocket / MQTT

## 快速开始

```bash
# 1. 克隆并进入项目
git clone [项目地址] && cd tc-iot-twetalk-esp-gmf-v2

# 2. 配置 ESP-IDF 环境
source /path/to/esp-idf/export.sh

# 3. 设置目标芯片并使用默认配置
idf.py set-target esp32s3
cp sdkconfig.defaults.esp32s3 sdkconfig

# 4. 配置 WiFi 和设备信息
idf.py menuconfig

# 5. 编译烧录监控
idf.py build flash monitor
```

### menuconfig 必要配置

| 配置路径 | 配置项 |
|---------|--------|
| `Example Connection Configuration` | WiFi SSID、WiFi Password |
| `Example Audio Configuration` | Product ID、Device Name、Device Secret、Calling Name、Calling OpenID |

> 设备认证信息从腾讯云物联网开发平台获取，详见接入指引文档。

## 硬件适配

**默认支持平台**：[立创·实战派 ESP32-S3](https://lckfb.com/project/detail/lckfb-esp32-s3-va?param=baseInfo&collection=71ac02fcf595444eb5dbbc45ae4ffda8)

其他开发板可在 `menuconfig → GMF APP Configuration → Target Board` 中选择。

**💡 自定义硬件建议**：将 `managed_components` 下的 `tempotian__codec_board` 和 `espressif__gmf_app_utils` 复制到 `components` 目录进行修改。

### 芯片兼容性

| 芯片 | 唤醒词模式 | 连续对话 | 按键模式 |
|------|:---:|:---:|:---:|
| ESP32-S3 | ✅ | ✅ | ✅ |
| ESP32-P4 | ✅ | ✅ | ✅ |
| ESP32 | ✅ | ✅ | ✅ |
| ESP32-C3/C6/S2 | ❌ | ❌ | ✅ |

## 音频编解码

TWeTalk 云服务规格：**OPUS / 16kHz / 单声道 / 24kbps / 60ms帧长**

支持格式：OPUS（推荐）、G.711A/U、PCM、AAC

## 工作模式

系统支持三种交互模式：

| 模式 | 说明 | 操作方式 |
|------|------|---------|
| **语音唤醒**（默认） | 唤醒词激活，低功耗 | 说 "Hi 乐鑫" → 听到提示音 → 开始对话，后续可连续对话 |
| **连续对话** | 无需唤醒，即时交互 | 直接说话，系统自动检测语音活动 |
| **按键触发** | 物理按键控制，零误触 | 按住 BOOT0 → 说话 → 松开结束 |

**模式切换**：双击 BOOT0 按键可在语音唤醒和按键模式之间切换。

唤醒词配置路径：`menuconfig → ESP Speech Recognition → use wakenet → Select wake words`

## 故障排查

### 自问自答问题

设备播放音频被麦克风重新采集导致误触发。调整以下参数：

```c
// main/audio_processor.c
#define DEFAULT_RECORD_DB        6    // 录音增益 (12 ~ 0)
#define DEFAULT_RECORD_REF_DB    26   // 参考信号增益 (30 ~ 20)
#define DEFAULT_PLAYBACK_VOLUME  60   // 播放音量 (40 ~ 80)
```

可通过 `menuconfig → Example Audio Configuration → Enable AEC Debug` 启用 AEC 调试（需 SD 卡）。

### I2C 总线共享

当其他模块也使用 I2C 时，通过 `esp_gmf_app_get_i2c_handle()` 获取已初始化的句柄，或在初始化时传入已有句柄：

```c
codec_info_t codec_info = { .i2c_handle = existing_handle };
esp_gmf_app_init(&codec_info);
```

## 技术支持

- [TWeTalk 官方文档](https://doc.weixin.qq.com/doc/w3_AJkAsAbTADsCNhIeEPMZsTXCETJk6?scode=AJEAIQdfAAot612P7vAJkAsAbTADs)
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)
- [ESP-GMF 框架](https://github.com/espressif/esp-gmf)