// RHI-GL: мясной бэкенд. C11. Ошибки — словами и со строками шейдеров.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1 /* glibc прячет Dl_info/dladdr за ним (T14) */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h> /* dladdr: резолв путей шейдеров от каталога .so (T14/N5) */
#include "rhi_gl.h"

void rhi_gl_hints(void) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

int rhi_gl_load(GLFWwindow *win) {
    glfwMakeContextCurrent(win);
    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "rhi_gl: glad fail\n");
        return 0;
    }
    if (!GLAD_GL_VERSION_4_5) {
        fprintf(stderr, "rhi_gl: need GL 4.5\n");
        return 0;
    }
    return 1;
}

// Каталог libengine.so (один раз, dladdr по собственному символу).
// Нужен чтобы относительные пути шейдеров работали из ЛЮБОГО CWD:
// раньше "engine/shaders/..." резолвился от текущей папки и ломал
// запуск редактора/демо не из корня репозитория (T14/N5).
static const char *rhi_gl_sodir(void) {
    static char dir[1024] = {0};
    static int tried = 0;
    if (!tried) {
        tried = 1;
        Dl_info info;
        if (dladdr((const void *)rhi_gl_prog, &info) && info.dli_fname) {
            char tmp[1024];
            snprintf(tmp, sizeof tmp, "%s", info.dli_fname);
            char *slash = strrchr(tmp, '/');
            if (slash) {
                *slash = 0;                 // .../engine/build -> .../engine
                char *eng = strrchr(tmp, '/');
                if (eng && strcmp(eng + 1, "build") == 0) {
                    *eng = 0;               // откат на каталог движка
                    snprintf(dir, sizeof dir, "%s/", tmp);
                } else {
                    snprintf(dir, sizeof dir, "%s/", tmp);
                }
            }
        }
    }
    return dir;
}

// Публичный резолв пути (T14/N5): как дан; от каталога .so (самый
// надёжный якорь — не зависит от CWD); от CWD вверх (до 6 уровней —
// запуск из template/*, tools/*, engine/build/*).
// Возвращает статический буфер с найденным путём или NULL.
// ПОРЯДОК ВАЖЕН (T17): сначала .so, потом upward-эскапация. Иначе при
// запуске из /tmp ложный хит "../../engine/..." мог победить настоящий
// каталог сборки.
const char *rhi_gl_find(const char *path) {
    static char cand[2048];
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); snprintf(cand, sizeof cand, "%s", path); return cand; }
    const char *sd = rhi_gl_sodir();
    if (sd[0]) {
        snprintf(cand, sizeof cand, "%s%s", sd, path);
        f = fopen(cand, "rb");
        if (f) { fclose(f); return cand; }
    }
    const char *roots[] = { "../", "../../", "../../../", "../../../../",
                            "../../../../../", "../../../../../../" };
    for (unsigned i = 0; i < sizeof roots / sizeof roots[0]; i++) {
        snprintf(cand, sizeof cand, "%s%s", roots[i], path);
        f = fopen(cand, "rb");
        if (f) { fclose(f); return cand; }
    }
    return 0;
}

// Один путь -> читаем через rhi_gl_find. Первый существующий.
static char *rhi_gl_read(const char *path) {
    const char *found = rhi_gl_find(path);
    if (!found) return 0;
    FILE *f = fopen(found, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = (char *)malloc((size_t)n + 1);
    if (!s) { fclose(f); return 0; }
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { free(s); fclose(f); return 0; }
    s[n] = 0;
    fclose(f);
    return s;
}

unsigned rhi_gl_prog(const char *vsPath, const char *fsPath) {
    char *vsc = rhi_gl_read(vsPath), *fsc = rhi_gl_read(fsPath);
    if (!vsc || !fsc) {
        fprintf(stderr, "rhi_gl: no shader %s / %s\n",
                vsc ? "?" : vsPath, fsc ? "?" : fsPath);
        free(vsc);
        free(fsc);
        return 0;
    }
    unsigned v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, (const char **)&vsc, 0);
    glCompileShader(v);
    int ok = 0;
    glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char l[2048];
        glGetShaderInfoLog(v, sizeof l, 0, l);
        fprintf(stderr, "rhi_gl vs %s: %s\n", vsPath, l);
    }
    unsigned f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, (const char **)&fsc, 0);
    glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char l[2048];
        glGetShaderInfoLog(f, sizeof l, 0, l);
        fprintf(stderr, "rhi_gl fs %s: %s\n", fsPath, l);
    }
    unsigned p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char l[2048];
        glGetProgramInfoLog(p, sizeof l, 0, l);
        fprintf(stderr, "rhi_gl link: %s\n", l);
        glDeleteProgram(p);
        p = 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    free(vsc);
    free(fsc);
    return p;
}

unsigned rhi_gl_empty_vao(void) {
    unsigned vao = 0;
    glGenVertexArrays(1, &vao);
    return vao;
}

const char *rhi_gl_err(void) {
    unsigned e = glGetError();
    if (!e) return "";
    if (e == 0x0500) return "INVALID_ENUM";
    if (e == 0x0501) return "INVALID_VALUE";
    if (e == 0x0502) return "INVALID_OPERATION";
    if (e == 0x0503) return "STACK_OVERFLOW";
    if (e == 0x0504) return "STACK_UNDERFLOW";
    if (e == 0x0505) return "OUT_OF_MEMORY";
    return "UNKNOWN";
}
