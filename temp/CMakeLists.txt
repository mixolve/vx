cmake_minimum_required(VERSION 3.22)

project(Vx VERSION 0.1.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(VX_PROJECT_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(VX_SOURCE_DIR "${VX_PROJECT_ROOT_DIR}/source")
set(VX_SHELL_DIR "${VX_SOURCE_DIR}/shell")
set(VX_SHARED_DIR "${VX_SOURCE_DIR}/shared")
set(VX_MODULES_DIR "${VX_SOURCE_DIR}/modules")
set(VX_EQE_MODULE_DIR "${VX_MODULES_DIR}/eqe")
set(VX_SPE_MODULE_DIR "${VX_MODULES_DIR}/spe")
set(VX_MXE_MODULE_DIR "${VX_MODULES_DIR}/mxe")
set(VX_TSE_MODULE_DIR "${VX_MODULES_DIR}/tse")
option(MIXOLVE_VX_ENABLE_EQE_APP_GROUP "Enable shared App Group EQE preset storage for iOS host app and AUv3 extension" ON)
option(MIXOLVE_VX_BUILD_MACOS_AU "Build the macOS Audio Unit format instead of VST3" OFF)

option(MIXOLVE_VX_USE_LOCAL_JUCE "Use a local JUCE checkout via JUCE_DIR" OFF)

function(vx_patch_juce_for_ios juce_source_dir)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()

    set(VX_JUCE_FILES_MAC "${juce_source_dir}/modules/juce_core/native/juce_Files_mac.mm")
    set(VX_JUCE_THREADS_MAC "${juce_source_dir}/modules/juce_core/native/juce_Threads_mac.mm")
    set(VX_JUCE_UTILS_CMAKE "${juce_source_dir}/extras/Build/CMake/JUCEUtils.cmake")
    set(VX_JUCE_JUCEAIDE_CMAKE "${juce_source_dir}/extras/Build/juceaide/CMakeLists.txt")

    if(EXISTS "${VX_JUCE_FILES_MAC}")
        file(READ "${VX_JUCE_FILES_MAC}" VX_JUCE_FILES_MAC_CONTENT)
        set(VX_JUCE_FILES_MAC_ORIGINAL_CONTENT "${VX_JUCE_FILES_MAC_CONTENT}")
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
"      #elif JUCE_IOS
        return false;"
"      #elif JUCE_IOS
        (void) filenameAsURL;
        return false;"
            VX_JUCE_FILES_MAC_CONTENT "${VX_JUCE_FILES_MAC_CONTENT}")
        string(REPLACE
            "JUCE_IOS && ! defined (__APPLICATION_EXTENSION__)"
            "JUCE_IOS && ! VX_APP_EXTENSION"
            VX_JUCE_FILES_MAC_CONTENT "${VX_JUCE_FILES_MAC_CONTENT}")

        if(NOT VX_JUCE_FILES_MAC_CONTENT STREQUAL VX_JUCE_FILES_MAC_ORIGINAL_CONTENT)
            file(WRITE "${VX_JUCE_FILES_MAC}" "${VX_JUCE_FILES_MAC_CONTENT}")
        endif()
    endif()

    if(EXISTS "${VX_JUCE_THREADS_MAC}")
        file(READ "${VX_JUCE_THREADS_MAC}" VX_JUCE_THREADS_MAC_CONTENT)
        set(VX_JUCE_THREADS_MAC_ORIGINAL_CONTENT "${VX_JUCE_THREADS_MAC_CONTENT}")
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

        if(NOT VX_JUCE_THREADS_MAC_CONTENT STREQUAL VX_JUCE_THREADS_MAC_ORIGINAL_CONTENT)
            file(WRITE "${VX_JUCE_THREADS_MAC}" "${VX_JUCE_THREADS_MAC_CONTENT}")
        endif()
    endif()

    if(EXISTS "${VX_JUCE_UTILS_CMAKE}")
        file(READ "${VX_JUCE_UTILS_CMAKE}" VX_JUCE_UTILS_CONTENT)
        set(VX_JUCE_UTILS_ORIGINAL_CONTENT "${VX_JUCE_UTILS_CONTENT}")
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

        if(NOT VX_JUCE_UTILS_CONTENT STREQUAL VX_JUCE_UTILS_ORIGINAL_CONTENT)
            file(WRITE "${VX_JUCE_UTILS_CMAKE}" "${VX_JUCE_UTILS_CONTENT}")
        endif()
    endif()

    if(EXISTS "${VX_JUCE_JUCEAIDE_CMAKE}")
        file(READ "${VX_JUCE_JUCEAIDE_CMAKE}" VX_JUCE_JUCEAIDE_CONTENT)
        set(VX_JUCE_JUCEAIDE_ORIGINAL_CONTENT "${VX_JUCE_JUCEAIDE_CONTENT}")
        string(REPLACE
"    set(PASSTHROUGH_ARGS \"\")"
"    set(PASSTHROUGH_ARGS \"\")

    if(CMAKE_SYSTEM_NAME STREQUAL \"iOS\")
        list(APPEND PASSTHROUGH_ARGS
            \"-DCMAKE_OSX_SYSROOT=macosx\"
            \"-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO\"
            \"-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO\")
    endif()"
            VX_JUCE_JUCEAIDE_CONTENT "${VX_JUCE_JUCEAIDE_CONTENT}")

        if(NOT VX_JUCE_JUCEAIDE_CONTENT STREQUAL VX_JUCE_JUCEAIDE_ORIGINAL_CONTENT)
            file(WRITE "${VX_JUCE_JUCEAIDE_CMAKE}" "${VX_JUCE_JUCEAIDE_CONTENT}")
        endif()
    endif()

endfunction()

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
    if(NOT DEFINED JUCE_DIR AND DEFINED ENV{JUCE_DIR})
        set(JUCE_DIR "$ENV{JUCE_DIR}")
    endif()

    if(NOT DEFINED JUCE_DIR OR JUCE_DIR STREQUAL "")
        message(FATAL_ERROR "Set JUCE_DIR (or environment variable JUCE_DIR) when MIXOLVE_VX_USE_LOCAL_JUCE=ON")
    endif()

    get_filename_component(VX_JUCE_DIR "${JUCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

    if(NOT EXISTS "${VX_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE_DIR does not point to a JUCE source tree: ${VX_JUCE_DIR}")
    endif()

    set(VX_JUCE_SOURCE_DIR "${VX_JUCE_DIR}")
    vx_patch_juce_for_ios("${VX_JUCE_SOURCE_DIR}")
    add_subdirectory("${VX_JUCE_SOURCE_DIR}" JUCE)
else()
    include(FetchContent)
    set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps")

    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
    )

    FetchContent_GetProperties(juce)
    if(NOT juce_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()

        FetchContent_Populate(juce)

        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()

    set(VX_JUCE_SOURCE_DIR "${juce_SOURCE_DIR}")
    vx_patch_juce_for_ios("${VX_JUCE_SOURCE_DIR}")
    add_subdirectory("${juce_SOURCE_DIR}" "${juce_BINARY_DIR}")
endif()

juce_add_binary_data(vx_assets
    SOURCES
    ${VX_PROJECT_ROOT_DIR}/resources/fonts/SometypeMono-Bold.ttf
    ${VX_PROJECT_ROOT_DIR}/resources/fonts/SometypeMono-Regular.ttf
    ${VX_PROJECT_ROOT_DIR}/resources/fonts/SometypeMono-OFL.txt
    ${VX_PROJECT_ROOT_DIR}/resources/about.md
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(VX_PLUGIN_FORMATS AUv3)
elseif(MIXOLVE_VX_BUILD_MACOS_AU)
    set(VX_PLUGIN_FORMATS AU)
else()
    set(VX_PLUGIN_FORMATS VST3)
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
        ${VX_MXE_MODULE_DIR}
        ${VX_SHARED_DIR}
        ${VX_SHELL_DIR}
        ${VX_EQE_MODULE_DIR}
        ${VX_SPE_MODULE_DIR}
        ${VX_TSE_MODULE_DIR}
)

file(GLOB VX_SHELL_HEADERS CONFIGURE_DEPENDS
    ${VX_SHELL_DIR}/*.h)

file(GLOB VX_SHELL_SOURCES CONFIGURE_DEPENDS
    ${VX_SHELL_DIR}/*.cpp)

set(VX_SHELL_SHARED_SOURCES ${VX_SHELL_SOURCES})

list(REMOVE_ITEM VX_SHELL_SHARED_SOURCES
    ${VX_SHELL_DIR}/IosHostMain.cpp)

file(GLOB VX_EQE_HEADERS CONFIGURE_DEPENDS
    ${VX_EQE_MODULE_DIR}/*.h)

file(GLOB VX_SHARED_HEADERS CONFIGURE_DEPENDS
    ${VX_SHARED_DIR}/*.h)

file(GLOB VX_EQE_SOURCES CONFIGURE_DEPENDS
    ${VX_EQE_MODULE_DIR}/*.cpp)

file(GLOB VX_EQE_OBJC_SOURCES CONFIGURE_DEPENDS
    ${VX_EQE_MODULE_DIR}/*.mm)

file(GLOB VX_SPE_BACKEND_HEADERS CONFIGURE_DEPENDS
    ${VX_SPE_MODULE_DIR}/*.h)

file(GLOB VX_SPE_BACKEND_SOURCES CONFIGURE_DEPENDS
    ${VX_SPE_MODULE_DIR}/*.cpp)

file(GLOB VX_MXE_HEADERS CONFIGURE_DEPENDS
    ${VX_MXE_MODULE_DIR}/*.h)

file(GLOB VX_MXE_PROCESSOR_SOURCES CONFIGURE_DEPENDS
    ${VX_MXE_MODULE_DIR}/*.cpp)

file(GLOB VX_TSE_HEADERS CONFIGURE_DEPENDS
    ${VX_TSE_MODULE_DIR}/*.h)

file(GLOB VX_TSE_SOURCES CONFIGURE_DEPENDS
    ${VX_TSE_MODULE_DIR}/*.cpp)

list(REMOVE_ITEM VX_SPE_BACKEND_SOURCES
    ${VX_SPE_MODULE_DIR}/module.spe.SpeProcessor.cpp)

list(APPEND VX_SPE_BACKEND_SOURCES
    ${VX_SPE_MODULE_DIR}/module.spe.SpeProcessor.cpp)

list(REMOVE_ITEM VX_MXE_PROCESSOR_SOURCES
    ${VX_MXE_MODULE_DIR}/module.mxe.PluginProcessor.cpp)

list(APPEND VX_MXE_PROCESSOR_SOURCES
    ${VX_MXE_MODULE_DIR}/module.mxe.PluginProcessor.cpp)

target_sources(Vx
    PRIVATE
        ${VX_EQE_HEADERS}
        ${VX_SHARED_HEADERS}
        ${VX_SHELL_HEADERS}
        ${VX_SHELL_SHARED_SOURCES}
        ${VX_EQE_SOURCES}
        ${VX_EQE_OBJC_SOURCES}
        ${VX_SPE_BACKEND_HEADERS}
        ${VX_SPE_BACKEND_SOURCES}
        ${VX_MXE_HEADERS}
        ${VX_MXE_PROCESSOR_SOURCES}
        ${VX_TSE_HEADERS}
        ${VX_TSE_SOURCES}
)

target_compile_definitions(Vx
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(Vx
        PUBLIC
            VX_APP_EXTENSION=1)
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

    if(MIXOLVE_VX_ENABLE_EQE_APP_GROUP)
        target_compile_definitions(Vx_AUv3
            PRIVATE
                VX_EQE_APP_GROUP_ENABLED=1)
    endif()

    set_target_properties(Vx_AUv3 PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM 4DHFZ3323A)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(VX_EQE_APP_GROUP_IDENTIFIER "group.com.mixolve.vx")
    set(VX_IOS_HOST_EXECUTABLE_NAME "vx")
    set(VX_IOS_HOST_BUNDLE_IDENTIFIER "com.mixolve.vx")
    set(VX_IOS_HOST_BUNDLE_NAME "vx")
    set(VX_IOS_HOST_DISPLAY_NAME "vx")

    configure_file(
        ${VX_PROJECT_ROOT_DIR}/temp/Vx_iOSHostInfo.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHostInfo.plist
        @ONLY)

    if(MIXOLVE_VX_ENABLE_EQE_APP_GROUP)
        configure_file(
            ${VX_PROJECT_ROOT_DIR}/temp/Vx_iOSHost.entitlements.in
            ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHost.entitlements
            @ONLY)

        configure_file(
            ${VX_PROJECT_ROOT_DIR}/temp/Vx_AUv3.entitlements.in
            ${CMAKE_CURRENT_BINARY_DIR}/Vx_AUv3.entitlements
            @ONLY)
    endif()

    set(VX_IOS_SHARED_ICON "${CMAKE_CURRENT_BINARY_DIR}/Icon.png")
    configure_file("${VX_PROJECT_ROOT_DIR}/resources/icon.png" "${VX_IOS_SHARED_ICON}" COPYONLY)
    set_source_files_properties("${VX_IOS_SHARED_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    juce_add_gui_app(Vx_iOSHost
        COMPANY_NAME "mixolve"
        BUNDLE_ID "${VX_IOS_HOST_BUNDLE_IDENTIFIER}"
        PRODUCT_NAME "vx")

    juce_generate_juce_header(Vx_iOSHost)

    target_include_directories(Vx_iOSHost
        PRIVATE
            ${VX_MXE_MODULE_DIR}
            ${VX_SHARED_DIR}
            ${VX_SHELL_DIR}
            ${VX_EQE_MODULE_DIR}
            ${VX_SPE_MODULE_DIR}
            ${VX_TSE_MODULE_DIR}
    )

    target_sources(Vx_iOSHost
        PRIVATE
            ${VX_EQE_HEADERS}
            ${VX_SHARED_HEADERS}
            ${VX_SHELL_HEADERS}
            ${VX_SHELL_SHARED_SOURCES}
            ${VX_EQE_SOURCES}
            ${VX_EQE_OBJC_SOURCES}
            ${VX_SPE_BACKEND_HEADERS}
            ${VX_SPE_BACKEND_SOURCES}
            ${VX_MXE_HEADERS}
            ${VX_MXE_PROCESSOR_SOURCES}
            ${VX_TSE_HEADERS}
            ${VX_TSE_SOURCES}
            ${VX_SHELL_DIR}/IosHostMain.cpp
            ${VX_IOS_SHARED_ICON}
    )

    target_compile_definitions(Vx_iOSHost
        PRIVATE
            JUCE_PLUGINHOST_AU=1
            JucePlugin_Name="vx"
            VX_APP_EXTENSION=0)

    if(MIXOLVE_VX_ENABLE_EQE_APP_GROUP)
        target_compile_definitions(Vx_iOSHost
            PRIVATE
                VX_EQE_APP_GROUP_ENABLED=1)
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

    if(TARGET Vx_AUv3 AND MIXOLVE_VX_ENABLE_EQE_APP_GROUP)
        set_target_properties(Vx_AUv3 PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_BINARY_DIR}/Vx_AUv3.entitlements)
    endif()

    set_target_properties(Vx_iOSHost PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHostInfo.plist
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM 4DHFZ3323A
        XCODE_EMBED_APP_EXTENSIONS Vx_AUv3)

    if(MIXOLVE_VX_ENABLE_EQE_APP_GROUP)
        set_target_properties(Vx_iOSHost PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_BINARY_DIR}/Vx_iOSHost.entitlements)
    endif()
endif()
