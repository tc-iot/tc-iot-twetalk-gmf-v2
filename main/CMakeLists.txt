
idf_component_register(
                        SRCS
                            main.c 
                            audio_process/audio_processor.c 
                            audio_process/audio_mixer_v2.c 
                            audio_process/audio_mixer.c 
                            audio_process/button_key.c
                            audio_process/pca9557.c
                       INCLUDE_DIRS "." "./audio_process" 
                       REQUIRES easy_button espressif__gmf_app_utils tempotian__codec_board 
                       PRIV_REQUIRES esp_netif esp_wifi nvs_flash)

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
