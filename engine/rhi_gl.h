#ifndef ENGINE_RHI_GL_H
#define ENGINE_RHI_GL_H

#ifdef __cplusplus
extern "C" {
#endif

// RHI-GL: сырой бэкенд OpenGL 4.5. Контекст, шейдеры с логами, UBO.
// Без сцены и контента: только устройство + проверка ошибок словами.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

// Хинты GL 4.5 core до glfwCreateWindow.
void rhi_gl_hints(void);
// Загрузить glad на окне. 1 ок, 0 нет (причина в stderr).
int rhi_gl_load(GLFWwindow *win);
// Программа из файлов VS+FS. 0 при ошибке (лог в stderr, со строками).
unsigned rhi_gl_prog(const char *vsPath, const char *fsPath);
// Резолв относительного пути ресурса: как дан; от CWD вверх (до 6 уровней);
// от каталога libengine.so. Возвращает путь для fopen или NULL если нигде
// нет. Нужен чтобы активы/шейдеры находились из ЛЮБОГО CWD (T14/N5).
const char *rhi_gl_find(const char *relPath);
// Пустой VAO (фулскрин по gl_VertexID). 0 при ошибке.
unsigned rhi_gl_empty_vao(void);
// Человеческий код последней ошибки GL. "" если чисто.
const char *rhi_gl_err(void);

#ifdef __cplusplus
}
#endif
#endif // ENGINE_RHI_GL_H
