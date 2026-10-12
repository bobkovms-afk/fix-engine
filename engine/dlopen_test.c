// Пруф встраивания: грузим libengine.so через dlopen (как редактор),
// сверяем версию, создаём ядро по имени и по (домен x бэкенд).
// Линкуется ТОЛЬКО с dl — ни одного символа движка на этапе сборки.
#define _GNU_SOURCE // dladdr + Dl_info (glibc требует feature-test макрос)
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_api.h" // только типы (caps/backend), линковки нет

typedef struct RenderCore RenderCore;
typedef RenderCore *(*rc_create_fn)(const char *);
typedef RenderCore *(*rc_create_by_fn)(unsigned, unsigned);
typedef const char *(*eng_version_fn)(void);

static void *try_open(void) {
    // N8: раньше — хардкод четырёх относительных путей, рабочий вариант
    // зависел от CWD запуска ctest. Теперь якорь = каталог самого теста
    // (dladdr, не зависит от CWD): libengine.so лежит рядом с этим
    // бинарником в build-дереве CMake. ENV LIBENGINE_PATH — явный override.
    void *h = 0;
    const char *env = getenv("LIBENGINE_PATH");
    if (env && env[0]) {
        h = dlopen(env, RTLD_NOW);
        if (h) { printf("opened env %s\n", env); return h; }
    }
    Dl_info info;
    if (dladdr((const void *)try_open, &info) && info.dli_fname) {
        char cand[2048];
        snprintf(cand, sizeof cand, "%s", info.dli_fname);
        char *sl = strrchr(cand, '/');
        if (sl) {
            strcpy(sl, "/libengine.so");
            h = dlopen(cand, RTLD_NOW);
            if (h) { printf("opened beside-exe %s\n", cand); return h; }
        }
    }
    // запасные относительные пути (ручной запуск из разных папок)
    static const char *paths[] = {
        "./libengine.so", "build/libengine.so",
        "engine/build/libengine.so",
    };
    for (int i = 0; i < 3; i++) {
        h = dlopen(paths[i], RTLD_NOW);
        if (h) { printf("opened %s\n", paths[i]); return h; }
    }
    return 0;
}

int main(void) {
    int bad = 0;
    void *h = try_open();
    if (!h) { printf("DLOPEN FAIL: no lib (%s)\n", dlerror()); return 1; }
    eng_version_fn ver = (eng_version_fn)dlsym(h, "eng_version");
    rc_create_fn mk = (rc_create_fn)dlsym(h, "rc_create");
    rc_create_by_fn mk2 = (rc_create_by_fn)dlsym(h, "rc_create_by");
    if (!ver || !mk || !mk2) { printf("DLOPEN FAIL: symbols\n"); return 1; }
    const char *v = ver();
    printf("engine version: %s (want 0.*)\n", v ? v : "?");
    if (!v || v[0] != '0' || v[1] != '.') { printf("DLOPEN FAIL: version\n"); bad = 1; }
    // rc_create лежит в lib (конструкторы ядер тоже там) — создаём vec.
    RenderCore *rc = mk("vec");
    printf("rc_create(vec)=%d (want 1)\n", !!rc);
    if (!rc) { printf("DLOPEN FAIL: vec\n"); bad = 1; }
    else if (rc->caps != RC_CAP_VECTOR || rc->backend != RC_BACKEND_GL ||
             !rc->name || strcmp(rc->name, "vec")) {
        printf("DLOPEN FAIL: fields caps=0x%x be=%u\n", rc->caps, rc->backend);
        bad = 1;
    } else {
        printf("fields OK: caps=0x%x backend=%u name=%s\n", rc->caps, rc->backend, rc->name);
    }
    RenderCore *mkv = mk2(RC_CAP_VOXEL, RC_BACKEND_VK);
    printf("rc_create_by(VOXEL,VK)=%d (want 1)\n", !!mkv);
    if (!mkv) { printf("DLOPEN FAIL: by\n"); bad = 1; }
    RenderCore *nope = mk("nope_does_not_exist");
    if (nope) { printf("DLOPEN FAIL: ghost\n"); bad = 1; }
    dlclose(h);
    printf(bad ? "DLOPEN FAIL\n" : "DLOPEN OK\n");
    return bad;
}
