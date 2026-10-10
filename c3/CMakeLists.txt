cmake_minimum_required(VERSION 3.16)

# 在 project() 之前注入默认证书包，这样 set-target 生成的 sdkconfig 会带上它
set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults")

# 固定版本号，别让它落回 git describe。启动横幅和烧录核对都看这个字符串。
set(PROJECT_VER "c3-dialogue-10s-1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ai-watch-c3)
