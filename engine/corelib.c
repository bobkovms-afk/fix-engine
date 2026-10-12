// Реестр ядер: каждое ядро само регистрируется конструктором,
// движок/игра выбирают по имени или по (домен × бэкенд), не зная состав.
// Линкуешь только pix_core — доступны только пиксели. Честная модульность.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render_api.h"

typedef RenderCore *(*rc_new_fn)(void);

#define RC_REG_CAP 32

static struct {
    const char *name;
    unsigned domain, backend;
    rc_new_fn fn;
} rc_reg[RC_REG_CAP];
static int rc_reg_n = 0;

void rc_register(const char *name, unsigned domain, unsigned backend, rc_new_fn fn) {
    // Молчаливый отказ здесь = «ядро не видно в редакторе» без единой
    // подсказки. Пишем причину в stderr: регистрация идёт конструктором,
    // человек этого места в коде не видит.
    if (!name || !fn) {
        fprintf(stderr, "[corelib] rc_register: NULL name/fn — пропущено\n");
        return;
    }
    if (rc_reg_n >= RC_REG_CAP) {
        fprintf(stderr, "[corelib] rc_register: реестр полон (%d), '%s' не добавлено\n",
                RC_REG_CAP, name);
        return;
    }
    for (int i = 0; i < rc_reg_n; i++)
        if (!strcmp(rc_reg[i].name, name)) return; // уже есть
    rc_reg[rc_reg_n].name = name;
    rc_reg[rc_reg_n].domain = domain;
    rc_reg[rc_reg_n].backend = backend;
    rc_reg[rc_reg_n].fn = fn;
    rc_reg_n++;
}

RenderCore *rc_create(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < rc_reg_n; i++)
        if (!strcmp(rc_reg[i].name, name)) return rc_reg[i].fn();
    fprintf(stderr, "[corelib] rc_create: ядро '%s' не зарегистрировано "
                    "(линковано в .so?)\n", name);
    return 0;
}

RenderCore *rc_create_by(unsigned domain, unsigned backend) {
    for (int i = 0; i < rc_reg_n; i++)
        if (rc_reg[i].domain == domain && rc_reg[i].backend == backend)
            return rc_reg[i].fn();
    // Пустая ячейка таблицы «домен x бэкенд» — штатный отказ (редактор
    // пишет пользователю), но причину («SDF под Vulkan нет») видно и в логе.
    fprintf(stderr, "[corelib] rc_create_by: нет ядра домен=%u бэкенд=%u\n",
            domain, backend);
    return 0;
}

const char *eng_version(void) {
    return "0.1.0";
}

void rc_destroy(RenderCore *rc) {
    if (!rc) return;
    // Wrap-структуры ядер аллоцируются в rc_*_create целиком (api внутри),
    // ctx указывает на сам wrap — освобождаем один раз, иначе каждое
    // создание-уничтожение ядра текло бы на размер обёртки.
    free(rc->ctx == (void *)rc ? (void *)rc : rc->ctx);
}
