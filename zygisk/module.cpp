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
#include <xhook.h> // Agora sim, o xHook oficial

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

static bool is_hidden_proc(const char* p) {
    if (!p) return false;
    for (auto& h : g_hide_procs) if (strstr(p, h.c_str())) return true;
    return false;
}

// ==================== HOOKS ====================
static int (*orig_open)(const char*, int, ...) = nullptr;
static struct dirent* (*orig_readdir)(DIR*) = nullptr;
static FILE* (*orig_fopen)(const char*, const char*) = nullptr;

// 1. Esconder arquivos
static int hook_open(const char* path, int flags, ...) {
    if (is_hidden(path)) { errno = ENOENT; return -1; }
    va_list a; va_start(a, flags);
    mode_t m = va_arg(a, mode_t); va_end(a);
    return orig_open(path, flags, m);
}

static FILE* hook_fopen(const char* path, const char* mode) {
    if (is_hidden(path)) return nullptr;
    return orig_fopen(path, mode);
}

// 2. Filtrar /proc/self/maps (esconde libs do PandaXE e do Root)
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
        bool hide = false;
        // Esconde paths configurados E libs do Pandaxe
        for (auto& h : g_hide_paths) if (line.find(h) != std::string::npos) { hide = true; break; }
        if (line.find("pandaxe") != std::string::npos || line.find("com.pandaxe") != std::string::npos) hide = true;
        
        if (!hide) filtered += line + "\n";
        pos = nl + 1;
    }

    int memfd = syscall(SYS_memfd_create, "maps", 0);
    if (memfd < 0) return real;
    write(memfd, filtered.data(), filtered.size());
    lseek(memfd, 0, SEEK_SET);
    return memfd;
}

static int hook_open_maps(const char* path, int flags, ...) {
    if (path && strcmp(path, "/proc/self/maps") == 0) return make_filtered_maps();
    va_list a; va_start(a, flags);
    mode_t m = va_arg(a, mode_t); va_end(a);
    return orig_open(path, flags, m);
}

// 3. Esconder processos (/proc)
static struct dirent* hook_readdir(DIR* dir) {
    struct dirent* e;
    while ((e = orig_readdir(dir)) != nullptr) {
        if (!is_hidden_proc(e->d_name)) return e;
    }
    return nullptr;
}

// ==================== INICIALIZAÇÃO ====================
__attribute__((constructor))
static void init_hooks() {
    load_config();
    if (g_hide_paths.empty() && g_hide_procs.empty()) return;

    // Aplica os hooks usando xHook
    xhook_register(".*\\.so$", "open", (void*)hook_open, (void**)&orig_open);
    xhook_register(".*\\.so$", "open", (void*)hook_open_maps, (void**)&orig_open);
    xhook_register(".*\\.so$", "fopen", (void*)hook_fopen, (void**)&orig_fopen);
    xhook_register(".*\\.so$", "readdir", (void*)hook_readdir, (void**)&orig_readdir);
    xhook_refresh(0);
}
