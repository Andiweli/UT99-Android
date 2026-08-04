#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstring>

#define LOG_TAG "UT99Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool mkdir_p(const char* path) {
    if (!path || !*path) {
        return false;
    }

    char tmp[1024];
    ::snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = ::strlen(tmp);
    if (len == 0 || len >= sizeof(tmp)) {
        return false;
    }

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (::mkdir(tmp, 0775) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }

    return (::mkdir(tmp, 0775) == 0 || errno == EEXIST);
}

static bool make_child_path(char* out, size_t outSize, const char* root, const char* child) {
    if (!out || outSize == 0 || !root || !child) {
        return false;
    }

    int written = ::snprintf(out, outSize, "%s/%s", root, child);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

static jboolean prepare_process_common(
        JNIEnv* env,
        jstring dataRootString,
        jstring homeDirString,
        jstring versionNameString) {
    if (!env || !dataRootString || !homeDirString) {
        LOGE("nativePrepareProcess called with null JNI arguments");
        return JNI_FALSE;
    }

    const char* dataRoot = env->GetStringUTFChars(dataRootString, nullptr);
    const char* homeDir = env->GetStringUTFChars(homeDirString, nullptr);
    const char* versionName = versionNameString
            ? env->GetStringUTFChars(versionNameString, nullptr)
            : nullptr;

    if (!dataRoot || !homeDir || (versionNameString && !versionName)) {
        LOGE("GetStringUTFChars failed: dataRoot=%p homeDir=%p", dataRoot, homeDir);
        if (dataRoot) {
            env->ReleaseStringUTFChars(dataRootString, dataRoot);
        }
        if (homeDir) {
            env->ReleaseStringUTFChars(homeDirString, homeDir);
        }
        if (versionName) {
            env->ReleaseStringUTFChars(versionNameString, versionName);
        }
        return JNI_FALSE;
    }

    bool ok = true;

    char systemDir[1024];
    char cacheDir[1024];
    char saveDir[1024];
    char logsDir[1024];
    if (!make_child_path(systemDir, sizeof(systemDir), dataRoot, "System") ||
        !make_child_path(cacheDir, sizeof(cacheDir), dataRoot, "Cache") ||
        !make_child_path(saveDir, sizeof(saveDir), dataRoot, "Save") ||
        !make_child_path(logsDir, sizeof(logsDir), dataRoot, "Logs")) {
        LOGE("Failed to build UT99 child paths from data root: %s", dataRoot ? dataRoot : "<null>");
        ok = false;
    }

    if (!mkdir_p(dataRoot)) {
        LOGE("Failed to create data root: %s", dataRoot);
        ok = false;
    }
    if (!mkdir_p(homeDir)) {
        LOGE("Failed to create home dir: %s", homeDir);
        ok = false;
    }
    if (ok && !mkdir_p(systemDir)) {
        LOGE("Failed to create System dir: %s", systemDir);
        ok = false;
    }
    if (ok && !mkdir_p(cacheDir)) {
        LOGE("Failed to create Cache dir: %s", cacheDir);
        ok = false;
    }
    if (ok && !mkdir_p(saveDir)) {
        LOGE("Failed to create Save dir: %s", saveDir);
        ok = false;
    }
    if (ok && !mkdir_p(logsDir)) {
        LOGE("Failed to create Logs dir: %s", logsDir);
        ok = false;
    }

    if (::setenv("HOME", homeDir, 1) != 0) {
        LOGE("setenv(HOME) failed: %s", ::strerror(errno));
        ok = false;
    }

    if (::setenv("UT99_ANDROID_DATA", dataRoot, 1) != 0) {
        LOGE("setenv(UT99_ANDROID_DATA) failed: %s", ::strerror(errno));
        ok = false;
    }

    // UT99_ANDROID_V200_RUNTIME_VERSION:
    // BuildConfig.VERSION_NAME is the authoritative APK version. Export it to
    // the native engine before SDL loads libUnrealTournament.so, so the menu
    // label can never be left behind by a stale CMake cache or hardcoded define.
    if (versionNameString) {
        if (versionName && *versionName) {
            if (::setenv("UT99_ANDROID_VERSION_NAME", versionName, 1) != 0) {
                LOGE("setenv(UT99_ANDROID_VERSION_NAME) failed: %s", ::strerror(errno));
                ok = false;
            } else {
                LOGI("Runtime build version set to %s", versionName);
            }
        } else {
            LOGE("Runtime build version is empty");
            ok = false;
        }
    }

    // appBaseDir() in the Unix/SDL platform layer is used very early by logging.
    // SDL_GetBasePath() can return null on Android/SDL2 in this embedded launch path,
    // so provide a stable System directory before SDL_main enters the Unreal launcher.
    if (::setenv("UT99_ANDROID_BASEDIR", systemDir, 1) != 0) {
        LOGE("setenv(UT99_ANDROID_BASEDIR) failed: %s", ::strerror(errno));
        ok = false;
    }

    // UT/UE1 normally runs from the System directory. This keeps relative paths such as
    // ../Maps, ../Textures and local User.ini/UnrealTournament.ini behaviour sane.
    if (::chdir(systemDir) != 0) {
        LOGE("chdir(%s) failed: %s", systemDir, ::strerror(errno));
        ok = false;
    } else {
        LOGI("Working directory set to %s", systemDir);
    }

    env->ReleaseStringUTFChars(dataRootString, dataRoot);
    env->ReleaseStringUTFChars(homeDirString, homeDir);
    if (versionName) {
        env->ReleaseStringUTFChars(versionNameString, versionName);
    }

    return ok ? JNI_TRUE : JNI_FALSE;
}

// v14 moved the SDL entry point from MainActivity to GameActivity. Keep both JNI
// names exported so older Java files and the current GameActivity both work.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_ut99_GameActivity_nativePrepareProcess(
        JNIEnv* env,
        jclass,
        jstring dataRootString,
        jstring homeDirString,
        jstring versionNameString) {
    return prepare_process_common(env, dataRootString, homeDirString, versionNameString);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ast_ut99_MainActivity_nativePrepareProcess(
        JNIEnv* env,
        jclass,
        jstring dataRootString,
        jstring homeDirString) {
    return prepare_process_common(env, dataRootString, homeDirString, nullptr);
}
