
idf_component_register(
                        SRCS
                            main.c 
                            audio_process/audio_processor.c 
                            audio_process/audio_mixer_v2.c 
                            audio_process/audio_mixer.c 
     
                            tc_twetalk/button_key.c
                            tc_twetalk/twetalk_app.c 
                            tc_twetalk/data_template_app.c 
                            tc_twetalk/pca9557.c
                       INCLUDE_DIRS "." "./audio_process" "./tc_twetalk"
                       REQUIRES easy_button espressif__gmf_app_utils tempotian__codec_board qcloud_iot_c_sdk
                       PRIV_REQUIRES esp_netif esp_wifi nvs_flash)

add_prebuilt_library(libiottwetalk "${CMAKE_CURRENT_SOURCE_DIR}/../components/qcloud_iot_c_sdk/lib/libtc_twetalk.a" PRIV_REQUIRES qcloud_iot_c_sdk)
target_link_libraries(${COMPONENT_LIB} PUBLIC libiottwetalk)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-missing-field-initializers")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-write-strings")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-unused-value") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-pointer-sign") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=dangling-pointer") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=address") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=enum-compare") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-maybe-uninitialized") 
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=int-conversion")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=incompatible-pointer-types")
