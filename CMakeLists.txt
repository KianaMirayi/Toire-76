cmake_minimum_required(VERSION 3.22)
project(TOIRE76 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────
# 前端开关
#   ON （默认）：WebView2 + WebUI/（网页前端，当前阶段）
#   OFF        ：原生占位 UI（只显示一行提示，用于没有 WebView2 的构建）
# 切换只改这一个选项；DSP / 参数层完全不动。
# ─────────────────────────────────────────────────────────────
option(TOIRE76_WEBVIEW2 "Build with the WebView2 backend (Web UI)" ON)

# ─────────────────────────────────────────────────────────────
# 原生 JUCE 渲染 POC（默认 OFF）
#   ON：额外构建一个 GUI demo（TOIRE76_NativeDemo），用同一批 WebUI/assets
#       素材 + 纯 juce::Graphics 画一遍 UI，用来和 WebView2 版比效果/体积/依赖。
#   不影响插件目标，也不进插件产物。
# ─────────────────────────────────────────────────────────────
option(TOIRE76_NATIVE_DEMO "Build the native-JUCE UI POC (for WebView2 comparison)" OFF)


add_subdirectory(../JUCE ${CMAKE_BINARY_DIR}/juce)

juce_add_plugin(TOIRE76
    COMPANY_NAME "TOREI"
    PLUGIN_MANUFACTURER_CODE Tore
    PLUGIN_CODE T761
    FORMATS VST3 Standalone
    PRODUCT_NAME "TOiRE-76"
    VERSION 0.1.0
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

# ⚠️ JUCE 8：必须在 target_sources 之前生成 JuceHeader.h
juce_generate_juce_header(TOIRE76)

target_sources(TOIRE76 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_include_directories(TOIRE76 PRIVATE src)

target_compile_definitions(TOIRE76 PUBLIC
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# ⚠️ MSVC 读 UTF-8 无 BOM 源码会按 GBK 解，中文注释里的 0x5C 会吞掉下一行
if(MSVC)
    target_compile_options(TOIRE76 PRIVATE /utf-8)
endif()

target_link_libraries(TOIRE76
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ─────────────────────────────────────────────────────────────
# WebView2 前端
#   NuGet 包里的 WebView2 SDK（build 是符号链接 -> 1.0.1901.177/build）
#   动态链接（JUCE_USE_WIN_WEBVIEW2=1）：运行时从 exe 旁边加载
#   WebView2Loader.dll，所以 DLL 必须和产物一起拷。
# ─────────────────────────────────────────────────────────────
if(TOIRE76_WEBVIEW2)
    set(WEBVIEW2_SDK "C:/Users/Admin/.nuget/packages/Microsoft.Web.WebView2")
    set(WEBVIEW2_INCLUDE "${WEBVIEW2_SDK}/build/native/include")
    set(WEBVIEW2_DLL "${WEBVIEW2_SDK}/build/native/x64/WebView2Loader.dll")

    if(NOT EXISTS "${WEBVIEW2_INCLUDE}/WebView2.h")
        message(FATAL_ERROR
            "找不到 WebView2 SDK 头文件：${WEBVIEW2_INCLUDE}/WebView2.h\n"
            "请确认 %USERPROFILE%\\.nuget\\packages\\Microsoft.Web.WebView2\\build 符号链接存在，"
            "或把 TOIRE76_WEBVIEW2 设为 OFF。")
    endif()

    target_include_directories(TOIRE76 PRIVATE "${WEBVIEW2_INCLUDE}")

    target_compile_definitions(TOIRE76 PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        TOIRE76_HAS_WEBVIEW=1)

    # WebBrowserComponent 在 juce_gui_extra 里（没有单独的 web_browser 模块）
    target_link_libraries(TOIRE76 PRIVATE juce::juce_gui_extra)

    set(WEBUI_SRC "${CMAKE_CURRENT_SOURCE_DIR}/WebUI")

    # ── Standalone ────────────────────────────────────────────
    #   Standalone/TOiRE-76.exe
    #   Standalone/webui/                       ← 前端
    #   Standalone/WebView2Loader.dll           ← 动态加载
    add_custom_command(TARGET TOIRE76_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEBUI_SRC}" "$<TARGET_FILE_DIR:TOIRE76_Standalone>/webui"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBVIEW2_DLL}" "$<TARGET_FILE_DIR:TOIRE76_Standalone>/WebView2Loader.dll"
        COMMENT "TOiRE-76: deploying webui/ + WebView2Loader.dll (Standalone)"
        VERBATIM)

    # ── VST3 ──────────────────────────────────────────────────
    #   VST3/TOiRE-76.vst3/Contents/x86_64-win/TOiRE-76.vst3
    #   VST3/TOiRE-76.vst3/Contents/webui/                    ← 前端
    #   VST3/TOiRE-76.vst3/Contents/x86_64-win/WebView2Loader.dll
    #
    #   $<TARGET_FILE_DIR:TOIRE76_VST3> 就是这个 x86_64-win 目录，
    #   所以 Contents/ 是它的上一级：<dir>/../webui
    add_custom_command(TARGET TOIRE76_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${WEBUI_SRC}" "$<TARGET_FILE_DIR:TOIRE76_VST3>/../webui"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBVIEW2_DLL}" "$<TARGET_FILE_DIR:TOIRE76_VST3>/WebView2Loader.dll"
        COMMENT "TOiRE-76: deploying webui/ + WebView2Loader.dll (VST3)"
        VERBATIM)

    # ── 顺手补一份到 %CommonProgramFiles%\VST3（COPY_PLUGIN_AFTER_BUILD 的目标）
    #
    #   JUCE 的 COPY_PLUGIN_AFTER_BUILD 只搬 .vst3 二进制 + moduleinfo，
    #   不会带 webui/ 和 WebView2Loader.dll —— DAW 从系统 VST3 目录加载时会黑屏。
    #   这里用生成的 cmake 脚本再补一次；**没有写权限就跳过**（不拖垮构建）。
    file(TO_CMAKE_PATH "$ENV{CommonProgramFiles}" TOIRE76_COMMON_PROGRAM_FILES)
    file(WRITE "${CMAKE_BINARY_DIR}/deploy_installed_vst3.cmake"
"set(WEBUI_SRC \"${WEBUI_SRC}\")
set(WEBVIEW2_DLL \"${WEBVIEW2_DLL}\")
set(DEST \"${TOIRE76_COMMON_PROGRAM_FILES}/VST3/TOiRE-76.vst3\")
execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_directory \"\${WEBUI_SRC}\" \"\${DEST}/Contents/webui\" RESULT_VARIABLE r1)
execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \"\${WEBVIEW2_DLL}\" \"\${DEST}/Contents/x86_64-win/WebView2Loader.dll\" RESULT_VARIABLE r2)
if(NOT r1 EQUAL 0 OR NOT r2 EQUAL 0)
    message(STATUS \"TOiRE-76: skip deploying webui into CommonProgramFiles (no write access)\")
endif()
")

    add_custom_command(TARGET TOIRE76_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/deploy_installed_vst3.cmake"
        COMMENT "TOiRE-76: best-effort deploy webui/ into CommonProgramFiles VST3"
        VERBATIM)

    message(STATUS "TOiRE-76: WebView2 frontend ENABLED (webui -> Standalone/ + VST3/Contents/)")
else()
    target_compile_definitions(TOIRE76 PUBLIC
        JUCE_WEB_BROWSER=0
        TOIRE76_HAS_WEBVIEW=0)
    message(STATUS "TOiRE-76: native placeholder UI (WebView2 disabled)")
endif()

# ─────────────────────────────────────────────────────────────
# 原生 JUCE 渲染 POC（TOIRE76_NATIVE_DEMO=ON 时构建）
#
#   用途：同一批素材（WebUI/assets）+ 同一份布局数据，用纯 juce::Graphics
#   画一遍，和 WebView2 版对比效果 / 体积 / 运行时依赖。
#
#   不链接任何 WebView 模块（juce_gui_extra 的 WebBrowserComponent 不会被拉进来），
#   所以产物不需要 WebView2Loader.dll，也不需要 WebView2 Runtime。
# ─────────────────────────────────────────────────────────────
if(TOIRE76_NATIVE_DEMO)
    juce_add_gui_app(TOIRE76_NativeDemo
        PRODUCT_NAME "TOiRE-76 Native Demo"
        COMPANY_NAME "TOREI"
        VERSION 0.1.0)

    # ⚠️ JUCE 8：必须在 target_sources 之前生成 JuceHeader.h
    juce_generate_juce_header(TOIRE76_NativeDemo)

    target_sources(TOIRE76_NativeDemo PRIVATE
        src/ui/NativeDemoMain.cpp
        src/ui/NativePanel.cpp)

    target_include_directories(TOIRE76_NativeDemo PRIVATE src)

    target_compile_definitions(TOIRE76_NativeDemo PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    if(MSVC)
        target_compile_options(TOIRE76_NativeDemo PRIVATE /utf-8)
    endif()

    target_link_libraries(TOIRE76_NativeDemo
        PRIVATE
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    # 素材不重新渲染：直接把 WebUI/assets 拷到 demo exe 旁边（assets/）
    add_custom_command(TARGET TOIRE76_NativeDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/assets"
            "$<TARGET_FILE_DIR:TOIRE76_NativeDemo>/assets"
        COMMENT "TOiRE-76: deploying WebUI/assets -> NativeDemo/assets"
        VERBATIM)

    message(STATUS "TOiRE-76: native render demo ENABLED (TOIRE76_NativeDemo)")
endif()

