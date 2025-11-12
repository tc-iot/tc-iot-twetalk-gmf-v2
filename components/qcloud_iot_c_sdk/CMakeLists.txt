message("make iot_c_sdk src")
set(TCIOT_SDK_SOURCE_DIR ${COMPONENT_DIR}/iot-hub-device-c-sdk)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -DPLATFORM_FREERTOS  -DPLATFORM_ESP_RTOS -DMBEDTLS_ALLOW_PRIVATE_ACCESS")

if(EXISTS ${TCIOT_SDK_SOURCE_DIR}/CMakeLists.txt)
    # 将代码提取到iot_c_sdk目录下
    message(STATUS "extract src code ")
    include(${TCIOT_SDK_SOURCE_DIR}/config/toolchains/esp_none.cmake)
    include(${TCIOT_SDK_SOURCE_DIR}/config/settings/twetalk_esp32.cmake)
endif()

file(GLOB src_explorer_sdk ${COMPONENT_DIR}/iot_explorer_sdk/src/*.c)
file(GLOB src_port ${COMPONENT_DIR}/port/*.c)

set(inc_explorer_sdk ${COMPONENT_DIR}/iot_explorer_sdk/inc)

# register twetalk sdk
idf_component_register(SRCS ${src_explorer_sdk} ${src_port}
    INCLUDE_DIRS ${inc_explorer_sdk} ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/inc
    PRIV_REQUIRES freertos lwip mbedtls bt nvs_flash app_update nvs_flash spi_flash
)

# 生成twetalk lib
if(EXISTS ${TCIOT_SDK_SOURCE_DIR}/CMakeLists.txt)
    set(src_twetalk ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/src/pipecat_pb.c 
                    ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/src/twetalk_mqtt.c 
                    ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/src/twetalk_msg.c 
                    ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/src/twetalk_ws_adapter.c 
                    ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/src/twetalk_ws.c )
    file(GLOB src_3rd ${TCIOT_SDK_SOURCE_DIR}/3rd/wslay/*.c)
    set(inc_3rd ${TCIOT_SDK_SOURCE_DIR}/3rd/wslay
                 ${TCIOT_SDK_SOURCE_DIR}/3rd/wslay/include)
    add_library(libtwetalk STATIC ${src_twetalk} ${src_3rd})
    target_include_directories(libtwetalk PUBLIC ${TCIOT_SDK_SOURCE_DIR}/component/aitalk/inc ${inc_explorer_sdk} ${inc_3rd})
    set(LIB_OUT_DIR "${TCIOT_SDK_SOURCE_DIR}/../lib")
    set_target_properties(libtwetalk PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUT_DIR}  # 关键设置
        OUTPUT_NAME "tc_twetalk"    
    )
endif()

set(src_lib_port ${COMPONENT_DIR}/port/HAL_File_free_rtos.c 
                  ${COMPONENT_DIR}/port/HAL_UDP_free_rtos.c 
                  ${COMPONENT_DIR}/port/HAL_TCP_free_rtos.c 
                  ${COMPONENT_DIR}/port/HAL_OS_free_rtos.c 
                  ${COMPONENT_DIR}/port/HAL_TLS_mbedtls.c 
                  ${COMPONENT_DIR}/port/HAL_Timer_free_rtos.c 
                  )

set(inc_lib_port ${COMPONENT_DIR}/iot_explorer_sdk/inc)

# 生成独立的libiothal库
add_library(libiothal STATIC ${src_lib_port})
target_include_directories(libiothal PUBLIC ${inc_lib_port})

# 链接系统库依赖
target_link_libraries(libiothal PUBLIC 
    idf::freertos 
    idf::lwip 
    idf::mbedtls
)

# 设置库输出目录和名称
set_target_properties(libiothal PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUT_DIR}  # 输出到lib目录
    OUTPUT_NAME "tc_iothal"                   # 输出文件名为libtc_iothal.a
)