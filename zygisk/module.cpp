#include <jni.h>
#include <zygisk.hpp>
#include <xhook.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

using zygisk::Api;
using zygisk::AppSpecializeArgs;

static std::vector<std::string> g_hide_paths;
static std::vector<std::string> g_hide_procs;
static std::string g_target_pkg;

// ==================== Config ====================
static void load_config() {
    FILE* f = fopen("/data/adb/hidehook/config.txt", "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        if (s.rfind("pkg:", 0) == 0)  g_target_pkg = s.substr(4);
        else if (s.rfind("path:", 0) == 0) g_hide_paths.push_back(s.substr(5));
        else if (s.rfind("proc:", 0) == 0) g_hide_procs.push_back(s.substr(5));
    }
    fclose(f);
}

static bool is_hidden(const char* p) {
    if (!p) return false;
    for (auto& h : g_hide_paths)
        if (strstr(p, h.c_str())) return true;
    return false;
}

// ==================== /proc/self/maps filtering ====================
static int (*orig_open)(const char*, int, ...) = nullptr;

static int make_filtered_maps() {
    int real = orig_open("/proc/self/maps", O_RDONLY);
    if (real < 0) return real;
    std::string content;
    char buf[65536];
    ssize_t n;
    while ((n = read(real, buf, sizeof(buf))) > 0)
        content.append(buf, n);
    close(real);

    std::string filtered;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        std::string line = content.substr(pos, nl - pos);
        bool hide = false;
        for (auto& h : g_hide_paths)
            if (line.find(h) != std::string::npos) { hide = true; break; }
        if (!hide) filtered += line + "\n";
        pos = nl + 1;
    }

    int memfd = syscall(SYS_memfd_create, "maps", 0);
    if (memfd < 0) return real;
    write(memfd, filtered.data(), filtered.size());
    lseek(memfd, 0, SEEK_SET);
    return memfd;
}

// ==================== Hooks ====================
static int hook_open(const char* path, int flags, ...) {
    if (is_hidden(path)) { errno = ENOENT; return -1; }
    if (path && strcmp(path, "/proc/self/maps") == 0)
        return make_filtered_maps();
    va_list a; va_start(a, flags);
    mode_t m = va_arg(a, mode_t); va_end(a);
    return orig_open(path, flags, m);
}

static struct dirent* (*orig_readdir)(DIR*) = nullptr;
static struct dirent* hook_readdir(DIR* dir) {
    struct dirent* e;
    while ((e = orig_readdir(dir)) != nullptr) {
        bool hide = false;
        for (auto& h : g_hide_procs)
            if (strstr(e->d_name, h.c_str())) { hide = true; break; }
        if (!hide) return e;
    }
    return nullptr;
}

// ==================== Zygisk entry ====================
class HideHookModule : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        const char* pkg = env->GetStringUTFChars(args->nice_name, nullptr);
        if (pkg) {
            g_target_pkg = pkg;
            env->ReleaseStringUTFChars(args->nice_name, pkg);
        }
        load_config();
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        // Só instala o hook se o app for o alvo que está no config
        if (g_target_pkg.empty()) return;

        // Sintaxe correta do xHook (diferente do que eu tinha mandado antes)
        xhook_register(".*\\.so$", "open", (void*)hook_open, (void**)&orig_open);
        xhook_register(".*\\.so$", "readdir", (void*)hook_readdir, (void**)&orig_readdir);
        xhook_refresh(0);
    }

private:
    Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(HideHookModule)
