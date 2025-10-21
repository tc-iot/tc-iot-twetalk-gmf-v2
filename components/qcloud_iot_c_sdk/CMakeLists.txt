message("make iot_c_sdk src")
set(IOT_SDK_SOURCE_DIR ${COMPONENT_DIR}/iot-hub-device-c-sdk)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -DPLATFORM_FREERTOS  -DPLATFORM_ESP_RTOS -DMBEDTLS_ALLOW_PRIVATE_ACCESS")

if(EXISTS ${IOT_SDK_SOURCE_DIR}/CMakeLists.txt)
    # 将代码提取到iot_c_sdk目录下
    message(STATUS "extract src code ")
    include(${IOT_SDK_SOURCE_DIR}/config/toolchains/esp_none.cmake)
    include(${IOT_SDK_SOURCE_DIR}/config/settings/twetalk_esp32.cmake)
endif()

file(GLOB src_explorer_sdk ${COMPONENT_DIR}/iot_explorer_sdk/src/*.c)
file(GLOB src_port ${COMPONENT_DIR}/port/*.c)

set(inc_explorer_sdk ${COMPONENT_DIR}/iot_explorer_sdk/inc)

# register twetalk sdk
idf_component_register(SRCS ${src_explorer_sdk} ${src_port}
    INCLUDE_DIRS ${inc_explorer_sdk}
    PRIV_REQUIRES freertos lwip mbedtls bt nvs_flash app_update nvs_flash spi_flash
)

# 生成twetalk lib
if(EXISTS ${IOT_SDK_SOURCE_DIR}/CMakeLists.txt)
    file(GLOB src_twetalk ${IOT_SDK_SOURCE_DIR}/component/twetalk/src/*.c)
    file(GLOB src_3rd ${IOT_SDK_SOURCE_DIR}/3rd/wslay/*.c)
    set(inc_3rd ${IOT_SDK_SOURCE_DIR}/3rd/wslay
                 ${IOT_SDK_SOURCE_DIR}/3rd/wslay/include)
    add_library(libtwetalk STATIC ${src_twetalk} ${src_3rd})
    target_include_directories(libtwetalk PUBLIC ${IOT_SDK_SOURCE_DIR}/component/twetalk/inc ${inc_explorer_sdk} ${inc_3rd})
    set(LIB_OUT_DIR "${IOT_SDK_SOURCE_DIR}/../lib")
    set_target_properties(libtwetalk PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUT_DIR}  # 关键设置
        OUTPUT_NAME "tc_twetalk"    
    )
endif()
