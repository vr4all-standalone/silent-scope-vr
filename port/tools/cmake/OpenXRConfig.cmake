# Import the unmodified cached Khronos1.1.43 ARM64 prefab module. Its bundled
# desktop config assumes a different installation layout; do not rewrite cache.
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "Silent Scope cached loader import requires Android ARM64")
endif()
if(NOT EXISTS "${SS_OPENXR_MODULE}/../headers/include/openxr/openxr.h" OR
   NOT EXISTS "${SS_OPENXR_MODULE}/libs/android.arm64-v8a/libopenxr_loader.so")
  message(FATAL_ERROR "Missing audited OpenXR prefab headers/library")
endif()
if(NOT TARGET OpenXR::openxr_loader)
  add_library(OpenXR::openxr_loader SHARED IMPORTED)
  set_target_properties(OpenXR::openxr_loader PROPERTIES
    IMPORTED_LOCATION "${SS_OPENXR_MODULE}/libs/android.arm64-v8a/libopenxr_loader.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SS_OPENXR_MODULE}/../headers/include")
endif()
set(OpenXR_FOUND TRUE)
