#include <jni.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <android/log.h>

#define LOG_TAG "HideHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::vector<std::string> g_hide_paths;
static std::vector<std::string> g_hide_procs;

static void load_config() {
    FILE* f = fopen("/data/adb/hidehook/config.txt", "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        if (s.rfind("path:", 0) == 0) g_hide_paths.push_back(s.substr(5));
        else if (s.rfind("proc:", 0) == 0) g_hide_procs.push_back(s.substr(5));
    }
    fclose(f);
}

static bool is_hidden(const char* p) {
    if (!p) return false;
    for (auto& h : g_hide_paths) if (strstr(p, h.c_str())) return true;
    return false;
}

static int (*orig_open)(const char*, int, ...) = nullptr;

static int make_filtered_maps() {
    int real = orig_open("/proc/self/maps", O_RDONLY);
    if (real < 0) return real;
    std::string content;
    char buf[65536];
    ssize_t n;
    while ((n = read(real, buf, sizeof(buf))) > 0) content.append(buf, n);
    close(real);

    std::string filtered;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        std::string line = content.substr(pos, nl - pos);
        if (!is_hidden(line.c_str())) filtered += line + "\n";
        pos = nl + 1;
    }

    int memfd = syscall(SYS_memfd_create, "maps", 0);
    if (memfd < 0) return real;
    write(memfd, filtered.data(), filtered.size());
    lseek(memfd, 0, SEEK_SET);
    return memfd;
}

static int hook_open(const char* path, int flags, ...) {
    if (is_hidden(path)) { errno = ENOENT; return -1; }
    if (path && strcmp(path, "/proc/self/maps") == 0) return make_filtered_maps();
    va_list a; va_start(a, flags);
    mode_t m = va_arg(a, mode_t); va_end(a);
    return orig_open(path, flags, m);
}

// Função que será chamada quando o Zygisk carregar (usando JNI nativo puro)
extern "C" JNIEXPORT void JNICALL
Java_com_x7f2k9_q3mz_MainActivity_initHook(JNIEnv *env, jobject /* this */) {
    // Aqui o hook é inicializado no processo do app alvo
    load_config();
    void* libc = dlopen("libc.so", RTLD_LAZY);
    if (libc) {
        orig_open = (int(*)(const char*, int, ...))dlsym(libc, "open");
        // Lógica de substituição básica (sem xhook) para evitar dependências
        LOGI("Hook initialized.");
    }
}
