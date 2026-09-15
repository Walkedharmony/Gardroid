if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(VCPKG_TARGET_TRIPLET "arm-neon-android" CACHE STRING "")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(VCPKG_TARGET_TRIPLET "arm64-android" CACHE STRING "")
elseif(ANDROID_ABI STREQUAL "x86")
    set(VCPKG_TARGET_TRIPLET "x86-android" CACHE STRING "")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(VCPKG_TARGET_TRIPLET "x64-android" CACHE STRING "")
endif()

# We expect VCPKG_ROOT to be passed or available in the environment
if(NOT DEFINED VCPKG_ROOT)
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
endif()

# Convert to forward slashes just in case
file(TO_CMAKE_PATH "${VCPKG_ROOT}" VCPKG_ROOT)

include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
