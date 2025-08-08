import pyogg
import wave

# pip install git+https://github.com/TeamPyOgg/PyOgg # 需要安装 PyOgg 库来处理 OPUS 格式

def opus_to_wav(opus_file_path, wav_file_path, sampling_rate=16000, channels=1, frame_duration_ms=20, bitrate=64000):
    """
    将 OPUS 文件转换为 WAV 文件，支持自定义采样率、通道数、帧时间和比特率。

    参数:
        opus_file_path (str): 输入的 OPUS 文件路径
        wav_file_path (str): 输出的 WAV 文件路径
        sampling_rate (int): 采样率（Hz），默认16kHz
        channels (int): 声道数，1为单声道，2为双声道，默认单声道
        frame_duration_ms (int): 每帧的时间长度（毫秒），默认20ms
        bitrate (int): 比特率，默认64000 bps
    """
    # 读取 OPUS 数据流并转换为 bytearray
    with open(opus_file_path, "rb") as file:
        opus_data = bytearray(file.read())

    # 计算每帧字节大小（根据比特率和帧持续时间）
    bytes_per_frame = int(bitrate / 8 * (frame_duration_ms / 1000))
    frames = [opus_data[i:i + bytes_per_frame] for i in range(0, len(opus_data), bytes_per_frame)]


    # 设置解码器
    opus_decoder = pyogg.OpusDecoder()
    opus_decoder.set_sampling_frequency(sampling_rate)
    opus_decoder.set_channels(channels)

    # 创建 WAV 文件
    with wave.open(wav_file_path, 'wb') as wavfile:
        wavfile.setparams((channels, int(16 / 8), sampling_rate, 0, 'NONE', 'NONE'))

        # 解码 OPUS 数据
        try:
            for frame in frames:
                decoded_frame = opus_decoder.decode(frame)
                wavfile.writeframes(decoded_frame)
            print(f"转换完成，文件已保存为 {wav_file_path}")

        except Exception as e:
            print("解码或转换过程中出错:", e)

def opus_with_len_to_wav(opus_file_path, wav_file_path, sampling_rate=16000, channels=1):
    """
    将 OPUS 文件转换为 WAV 文件，支持自定义采样率、通道数、帧时间和比特率。

    参数:
        opus_file_path (str): 输入的 OPUS 文件路径
        wav_file_path (str): 输出的 WAV 文件路径
        sampling_rate (int): 采样率（Hz），默认16kHz
        channels (int): 声道数，1为单声道，2为双声道，默认单声道
    """
    # 读取 OPUS 数据流并转换为 bytearray
    with open(opus_file_path, "rb") as file:
        opus_data = bytearray(file.read())

    pos_now = 0
    frames = []
    while True:
        frame_len = 180
        # print(frame_len, opus_data[pos_now + 1])
        frames.append(opus_data[pos_now: pos_now + frame_len])
        pos_now += frame_len
        if pos_now >= len(opus_data):
            break

    # 设置解码器
    opus_decoder = pyogg.OpusDecoder()
    opus_decoder.set_sampling_frequency(sampling_rate)
    opus_decoder.set_channels(channels)

    with wave.open(wav_file_path, 'wb') as pcmfile:
        pcmfile.setnchannels(1)
        pcmfile.setsampwidth(2)  # 16-bit PCM
        pcmfile.setframerate(16000)  # 采样率
        # 解码 OPUS 数据
        try:
            for frame in frames:
                decoded_frame = opus_decoder.decode(frame)
                pcmfile.writeframes(decoded_frame)  # 直接将解码后的 PCM 数据写入文件
            print(f"转换完成，文件已保存为 {wav_file_path}")

        except Exception as e:
            print("解码或转换过程中出错:", e)


if __name__ == "__main__":
    # opus_to_wav("../files/test.opus", "../wav/output.wav", sampling_rate=16000, channels=1, frame_duration_ms=60, bitrate=64000)
    opus_with_len_to_wav("./output/recv.opus", "./output/output.wav", sampling_rate=16000, channels=1)