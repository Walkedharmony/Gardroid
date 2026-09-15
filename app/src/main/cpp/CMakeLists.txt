# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("gardroid")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
add_library(${CMAKE_PROJECT_NAME} SHARED
        bridge/jni_archive_bridge.cpp
        bridge/jni_text_bridge.cpp
        bridge/jni_audio_bridge.cpp
        bridge/ArchiveFactory.cpp
        utils/StringUtils.cpp
        formats/Xp3Parser.cpp
        formats/Xp3Packer.cpp
        utils/TlgDecoder.cpp
        utils/AudioDecoder.cpp
        utils/BgiImageDecoder.cpp
        utils/AstParser.cpp
        utils/PSBFILE/compress.c
        utils/PSBFILE/psb.cpp
        utils/PSBFILE/ScnParser.cpp
        utils/KirikiriKS.cpp
        utils/AstPacker.cpp
        formats/ArcParser.cpp
        formats/PfsParser.cpp
        formats/BgiParser.cpp
        formats/PfsPacker.cpp)

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
find_package(ZLIB REQUIRED)
find_package(OpusFile CONFIG REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME}
    ZLIB::ZLIB
    Ogg::ogg
    Opus::opus
    OpusFile::opusfile
    android
    log)