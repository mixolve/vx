cmake_minimum_required(VERSION 3.22)

project(Vx VERSION 0.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(VX_PROJECT_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(VX_SOURCE_DIR "${VX_PROJECT_ROOT_DIR}/source")
set(VX_SHELL_DIR "${VX_SOURCE_DIR}/shell")
set(VX_MODULES_DIR "${VX_SOURCE_DIR}/modules")
set(VX_EQE_MODULE_DIR "${VX_MODULES_DIR}/eqe")
set(VX_EQE_HEADERS_DIR "${VX_EQE_MODULE_DIR}/headers")
set(VX_EQE_EDITOR_DIR "${VX_EQE_MODULE_DIR}/editor")
set(VX_EQE_PROCESSOR_DIR "${VX_EQE_MODULE_DIR}/processor")

option(MIXOLVE_VX_USE_LOCAL_JUCE "Use a local JUCE checkout via JUCE_DIR" OFF)
option(MIXOLVE_VX_ENABLE_APP_GROUP "Enable shared App Group preset storage for the iOS host app and AUv3 extension" ON)

if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS version" FORCE)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "iOS architectures" FORCE)
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures" FORCE)
    endif()
endif()

if(MIXOLVE_VX_USE_LOCAL_JUCE)
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "Set -DJUCE_DIR=/absolute/path/to/JUCE when MIXOLVE_VX_USE_LOCAL_JUCE=ON")
    endif()

    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps")

    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
    )

    FetchContent_MakeAvailable(juce)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(VX_JUCE_FILES_MAC "${FETCHCONTENT_BASE_DIR}/juce-src/modules/juce_core/native/juce_Files_mac.mm")
    set(VX_JUCE_THREADS_MAC "${FETCHCONTENT_BASE_DIR}/juce-src/modules/juce_core/native/juce_Threads_mac.mm")
    set(VX_JUCE_UTILS_CMAKE "${FETCHCONTENT_BASE_DIR}/juce-src/extras/Build/CMake/JUCEUtils.cmake")

    if(EXISTS "${VX_JUCE_FILES_MAC}")
        file(READ "${VX_JUCE_FILES_MAC}" VX_JUCE_FILES_MAC_CONTENT)
        string(REPLACE
"      #if JUCE_IOS
        [[UIApplication sharedApplication] openURL: filenameAsURL
                                           options: @{}
                                 completionHandler: nil];

        return true;
      #else"
"      #if JUCE_IOS && ! VX_APP_EXTENSION
        [[UIApplication sharedApplication] openURL: filenameAsURL
                                           options: @{}
                                 completionHandler: nil];

        return true;
      #elif JUCE_IOS
        return false;
      #else"
            VX_JUCE_FILES_MAC_CONTENT "${VX_JUCE_FILES_MAC_CONTENT}")
        string(REPLACE
            "JUCE_IOS && ! defined (__APPLICATION_EXTENSION__)"
            "JUCE_IOS && ! VX_APP_EXTENSION"
            VX_JUCE_FILES_MAC_CONTENT "${VX_JUCE_FILES_MAC_CONTENT}")
        file(WRITE "${VX_JUCE_FILES_MAC}" "${VX_JUCE_FILES_MAC_CONTENT}")
    endif()

    if(EXISTS "${VX_JUCE_THREADS_MAC}")
        file(READ "${VX_JUCE_THREADS_MAC}" VX_JUCE_THREADS_MAC_CONTENT)
        string(REPLACE
"       #elif JUCE_IOS
        [[UIApplication sharedApplication] performSelector: @selector (suspend)];"
"       #elif JUCE_IOS && ! VX_APP_EXTENSION
        [[UIApplication sharedApplication] performSelector: @selector (suspend)];"
            VX_JUCE_THREADS_MAC_CONTENT "${VX_JUCE_THREADS_MAC_CONTENT}")
        string(REPLACE
            "JUCE_IOS && ! defined (__APPLICATION_EXTENSION__)"
            "JUCE_IOS && ! VX_APP_EXTENSION"
            VX_JUCE_THREADS_MAC_CONTENT "${VX_JUCE_THREADS_MAC_CONTENT}")
        file(WRITE "${VX_JUCE_THREADS_MAC}" "${VX_JUCE_THREADS_MAC_CONTENT}")
    endif()

    if(EXISTS "${VX_JUCE_UTILS_CMAKE}")
        file(READ "${VX_JUCE_UTILS_CMAKE}" VX_JUCE_UTILS_CONTENT)
        string(REPLACE
"    if(CMAKE_SYSTEM_NAME STREQUAL \"iOS\")
        get_target_property(add_storyboard \${source_target} JUCE_SHOULD_ADD_STORYBOARD)

        if(add_storyboard)"
"    if(CMAKE_SYSTEM_NAME STREQUAL \"iOS\")
        get_target_property(add_storyboard \${source_target} JUCE_SHOULD_ADD_STORYBOARD)

        if(juce_kind_string STREQUAL \"AUv3 AppExtension\")
            set(add_storyboard FALSE)
        endif()

        if(add_storyboard)"
            VX_JUCE_UTILS_CONTENT "${VX_JUCE_UTILS_CONTENT}")
        string(REPLACE
            "    set(needs_storyboard TRUE)"
            "    set(needs_storyboard FALSE)"
            VX_JUCE_UTILS_CONTENT "${VX_JUCE_UTILS_CONTENT}")
        file(WRITE "${VX_JUCE_UTILS_CMAKE}" "${VX_JUCE_UTILS_CONTENT}")
        include("${VX_JUCE_UTILS_CMAKE}")
    endif()
endif()

juce_add_binary_data(vx_assets
    SOURCES
    ${VX_PROJECT_ROOT_DIR}/resources/fonts/SometypeMono-Bold.ttf
    ${VX_PROJECT_ROOT_DIR}/resources/fonts/SometypeMono-Regular.ttf
    ${VX_PROJECT_ROOT_DIR}/resources/fonts/SometypeMono-OFL.txt
        ${VX_PROJECT_ROOT_DIR}/resources/about.md
)

set(VX_PLUGIN_FORMATS VST3)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND VX_PLUGIN_FORMATS AUv3)
elseif(APPLE)
    list(APPEND VX_PLUGIN_FORMATS AU)
endif()

set(VX_PLUGIN_ARGS
    COMPANY_NAME "mixolve"
    BUNDLE_ID "com.mixolve.vx"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Mxol
    PLUGIN_CODE Vx01
    FORMATS ${VX_PLUGIN_FORMATS}
    PRODUCT_NAME "vx"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "This app needs microphone access to process audio."
)

juce_add_plugin(Vx ${VX_PLUGIN_ARGS})

juce_generate_juce_header(Vx)

target_include_directories(Vx
    PRIVATE
        ${VX_SHELL_DIR}
        ${VX_EQE_HEADERS_DIR}
)

file(GLOB VX_EQE_HEADERS CONFIGURE_DEPENDS
    ${VX_EQE_HEADERS_DIR}/*.h)

file(GLOB VX_EQE_EDITOR_SOURCES CONFIGURE_DEPENDS
    ${VX_EQE_EDITOR_DIR}/*.cpp)

file(GLOB VX_EQE_PROCESSOR_SOURCES CONFIGURE_DEPENDS
    ${VX_EQE_PROCESSOR_DIR}/*.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND MIXOLVE_VX_ENABLE_APP_GROUP)
    list(APPEND VX_EQE_PROCESSOR_SOURCES
        ${VX_EQE_PROCESSOR_DIR}/AppGroupStorage.mm)
endif()

target_sources(Vx
    PRIVATE
        ${VX_EQE_HEADERS}
        ${VX_EQE_EDITOR_SOURCES}
        ${VX_EQE_PROCESSOR_SOURCES}
)

target_compile_definitions(Vx
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND MIXOLVE_VX_ENABLE_APP_GROUP)
    target_compile_definitions(Vx
        PUBLIC
            VX_APP_GROUP_ENABLED=1)
endif()

target_link_libraries(Vx
    PRIVATE
        vx_assets
        juce::juce_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(TARGET Vx_AUv3)
    target_link_libraries(Vx_AUv3
        PRIVATE
            vx_assets
            juce::juce_dsp)

    target_compile_definitions(Vx_AUv3
        PRIVATE
            VX_APP_EXTENSION=1)

    if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND MIXOLVE_VX_ENABLE_APP_GROUP)
        target_compile_definitions(Vx_AUv3
            PRIVATE
                VX_APP_GROUP_ENABLED=1)
    endif()

    set_target_properties(Vx_AUv3 PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM 4DHFZ3323A)
endif()

if(TARGET Vx_AU)
    target_link_libraries(Vx_AU
        PRIVATE
            vx_assets
            juce::juce_dsp)

    set_target_properties(Vx_AU PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM 4DHFZ3323A)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(VX_APP_GROUP_IDENTIFIER "group.com.mixolve.vx")
    set(VX_IOS_HOST_EXECUTABLE_NAME "vx-app")
    set(VX_IOS_HOST_BUNDLE_IDENTIFIER "com.mixolve.vx")
    set(VX_IOS_HOST_BUNDLE_NAME "vx-app")
    set(VX_IOS_HOST_DISPLAY_NAME "vx-app")

    configure_file(
        ${VX_PROJECT_ROOT_DIR}/temp/Vx_iOSHostInfo.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHostInfo.plist
        @ONLY)

    if(MIXOLVE_VX_ENABLE_APP_GROUP)
        configure_file(
            ${VX_PROJECT_ROOT_DIR}/temp/Vx_iOSHost.entitlements.in
            ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHost.entitlements
            @ONLY)

        configure_file(
            ${VX_PROJECT_ROOT_DIR}/temp/Vx_AUv3.entitlements.in
            ${CMAKE_CURRENT_BINARY_DIR}/Vx_AUv3.entitlements
            @ONLY)
    endif()

    if(TARGET Vx_AUv3 AND MIXOLVE_VX_ENABLE_APP_GROUP)
        set_target_properties(Vx_AUv3 PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_BINARY_DIR}/Vx_AUv3.entitlements)
    endif()

    juce_add_gui_app(Vx_iOSHost
        COMPANY_NAME "mixolve"
        BUNDLE_ID "com.mixolve.vx"
        PRODUCT_NAME "vx-app")

    set(VX_IOS_APP_ICON_SOURCE "${VX_PROJECT_ROOT_DIR}/resources/icon.png")
    set(VX_IOS_APP_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHostAppIcons")
    file(MAKE_DIRECTORY "${VX_IOS_APP_ICON_DIR}")

    set(VX_IOS_APP_ICON_FILES)

    function(vx_make_ios_icon icon_name icon_size)
        set(icon_path "${VX_IOS_APP_ICON_DIR}/${icon_name}")
        execute_process(
            COMMAND /usr/bin/sips -s format png "${VX_IOS_APP_ICON_SOURCE}" --out "${icon_path}" -z "${icon_size}" "${icon_size}"
            RESULT_VARIABLE icon_result
            OUTPUT_QUIET
            ERROR_QUIET)

        if(NOT icon_result EQUAL 0)
            message(FATAL_ERROR "Unable to generate iOS app icon: ${icon_path}")
        endif()

        set(VX_IOS_APP_ICON_FILES ${VX_IOS_APP_ICON_FILES} "${icon_path}" PARENT_SCOPE)
    endfunction()

    vx_make_ios_icon("Icon-20.png" 20)
    vx_make_ios_icon("Icon-20@2x.png" 40)
    vx_make_ios_icon("Icon-20@3x.png" 60)
    vx_make_ios_icon("Icon-29.png" 29)
    vx_make_ios_icon("Icon-29@2x.png" 58)
    vx_make_ios_icon("Icon-29@3x.png" 87)
    vx_make_ios_icon("Icon-40.png" 40)
    vx_make_ios_icon("Icon-40@2x.png" 80)
    vx_make_ios_icon("Icon-40@3x.png" 120)
    vx_make_ios_icon("Icon-60@2x.png" 120)
    vx_make_ios_icon("Icon-60@3x.png" 180)
    vx_make_ios_icon("Icon-76.png" 76)
    vx_make_ios_icon("Icon-76@2x.png" 152)
    vx_make_ios_icon("Icon-83.5@2x.png" 167)

    foreach(icon_file IN LISTS VX_IOS_APP_ICON_FILES)
        add_custom_command(TARGET Vx_iOSHost POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${icon_file}"
                "$<TARGET_BUNDLE_DIR:Vx_iOSHost>")
    endforeach()

    juce_generate_juce_header(Vx_iOSHost)

    target_include_directories(Vx_iOSHost
        PRIVATE
            ${VX_SHELL_DIR}
            ${VX_EQE_HEADERS_DIR}
    )

    target_sources(Vx_iOSHost
        PRIVATE
            ${VX_EQE_HEADERS}
            ${VX_EQE_EDITOR_SOURCES}
            ${VX_EQE_PROCESSOR_SOURCES}
            ${VX_SHELL_DIR}/IosHostMain.cpp
    )

    target_compile_definitions(Vx_iOSHost
        PRIVATE
            JucePlugin_Name="vx"
    )

    if(MIXOLVE_VX_ENABLE_APP_GROUP)
        target_compile_definitions(Vx_iOSHost
            PRIVATE
                VX_APP_GROUP_ENABLED=1)
    endif()

    target_link_libraries(Vx_iOSHost
        PRIVATE
            vx_assets
            juce::juce_audio_processors
            juce::juce_dsp
            juce::juce_audio_utils
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    if(TARGET Vx_AUv3)
        add_dependencies(Vx_iOSHost Vx_AUv3)
    endif()

    set_target_properties(Vx_iOSHost PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHostInfo.plist
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM 4DHFZ3323A
        XCODE_EMBED_APP_EXTENSIONS Vx_AUv3)

    if(MIXOLVE_VX_ENABLE_APP_GROUP)
        set_target_properties(Vx_iOSHost PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHost.entitlements)
    endif()
endif()
