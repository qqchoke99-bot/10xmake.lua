#pragma once
#include <android/log.h>

#define SPL_TAG "SoundPhysicsLite"
#define SPL_LOGI(...) __android_log_print(ANDROID_LOG_INFO, SPL_TAG, __VA_ARGS__)
#define SPL_LOGW(...) __android_log_print(ANDROID_LOG_WARN, SPL_TAG, __VA_ARGS__)
#define SPL_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SPL_TAG, __VA_ARGS__)
