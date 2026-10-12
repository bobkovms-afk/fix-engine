# Карта проекта + план исправлений

Не коммитить в артефакты сборки; рабочий файл. Галочка = починено и проверено.

## КАРТА (что от чего зависит)

```
/workspace
├── engine/                  libengine.so (единый ship-артефакт, C11)
│   ├── render_api.h         L0 КОНТРАКТ: RenderCore, RcView, домены RC_CAP_*, бэкенды RC_BACKEND_*
│   ├── engine_api.h         L0: EngEngine (окно+ввод+цикл), eng_create(rc,...) — rc создаёт ИГРА
│   ├── corelib.c            реестр ядер rc_register/rc_create_by + eng_version + rc_destroy
│   ├── rhi_gl.c/rhi_vk.c/rhi_webgpu.c  L1 прослойки GPU-API
│   ├── gl_core vk_core webgpu_core sdf_core mesh_core pix_core vec_core  L2 ядра (домен×бэкенд)
│   │     └ каждый саморегистрируется __attribute__((constructor))
│   ├── comp.c/h             L3 композитор проходов (нужен frame_to у всех)
│   ├── rtarget.h comp_gl.h  L3 цели (FBO под GL)
│   ├── vox/ vox_feed.c      L4 воксельный домен (ген, меш, текстуры, фидер)
│   ├── sky_lut.c asset.c scene.c mat4.h vec.h phys.h vfs.h
│   ├── shaders/sdf+sky (.wgsl) -> embed_wgsl.cmake -> build/gen/sdf_wgsl.h (embed в .so)
│   ├── shaders/vk (.glsl)  -> glslc -> build/spv_vk/*.spv (НА ВРЕМЯ РАНЛАЙТА по VK_SPV_DIR!)
│   ├── shaders/gl,mesh,blit (.glsl) -> rhi_gl_prog читает с диска при init
│   ├── third-party/glad, Vulkan-Headers
│   └── CMakeLists.txt       единственная сборочная система (второй build-системы нет)
├── frontend/                editor (Qt6 Widgets, C++17)
│   ├── main.cpp             openEditor(): меню/тулбары/доки; domainForTemplate() парсит project.txt
│   ├── hub.cpp              хаб проектов (ED_HUB_ADD, карточки, выбор бэкенда)
│   ├── viewport.cpp         мост движок↔визуал: скрытое GLFW-окно, rc_create_by(dom,be),
│   │                        GL: frame_to→FBO→glReadPixels; VK/WGPU: frame_cpu→RGBA→QImage
│   ├── panels.cpp           глобальный g_world (ScWorld), инспектор, сейв scene.txt
│   └── CMakeLists.txt       линкует ../engine/build/libengine.so (rutime-путь через rpath)
├── template/                демо поверх API: scene3d (общая сцена для всех ядер), scene2d,
│                            comp_demo, sand_demo, lua_demo, comp_demo
└── tools/                   run.sh (движок), editor.sh (редактор), mkdemo.sh (сборка+генерация демо), matrix.sh
```

Маршруты:
- Сборка: tools/*.sh → mkdemo.sh::mkbuild → cmake engine → libengine.so → cmake frontend → editor.
- Кадр редактора: QTimer → Viewport::renderOnce → RcView(собрать вручную) → rc->frame_to/frame_cpu → QImage → paintEvent.
- Выбор ядра: проект(project.txt template=) → домен → rc_create_by(domain,backend) из реестра конструкторов.
- Контент редактору: хардкор в viewport.cpp (vox_gen 5x5 сид 1337); scene.txt/ScWorld на рендер НЕ влияет.

## ПЛАН ИСПРАВЛЕНИЙ

### P0 — контракты и подключение движка к визуалу
- [x] T1 `rc_destroy` не освобождает сам RenderCore (утечка каждого ядра; Viewport::initDomain при FAILED теряет newRc). Правим corelib.c. ПРОВЕРЕНО 2-й круг: corelib.c:67 free() корректен, viewport.cpp вызывает rc_destroy на всех error-ветках (строки 82,91,101,104,124,127).
- [x] T2 Реестр: молчаливый отказ при переполнении (cap 16) и NULL-ах → явный stderr-лог. ПРОВЕРЕНО: corelib.c RC_REG_CAP=32 + fprintf на NULL/полный реестр/нет ядра.
- [x] T3 front/back контракт backend: магические числа 1/2/3 в main.cpp/hub/viewport → константы RC_BACKEND_*. ПРОВЕРЕНО: viewport.cpp использует RC_BACKEND_GL/VK/WEBGPU везде.
- [x] T4 domainForTemplate возвращает сырые int → константы RC_CAP_*; неизвестный шаблон логируется. ПРОВЕРЕНО: main.cpp:60-84 только RC_CAP_*, stderr-лог неизвестного шаблона.
- [ ] T5 view-параметры редактора: VOX_DAYT из env внутри renderOnce → поле EdConfig (дефолт без env), env только как CLI override. НЕ СДЕЛАНО: viewport.cpp:402 по-прежнему getenv("VOX_DAYT") каждый кадр.
- [ ] T6 Разъединить два «scene»: UI-мир ScWorld (panels) не влияет на рендер. Минимум: feed воксельного ядра учитывает трансформ выбранной сущности (гизмо реально дёргает картинку) ИЛИ честная пометка. Проверяем по коду, выбираем дешёвый честный вариант + запись в консоль.

### P1 — сборка/ресурсы
- [ ] T7 Шейдеры blit грузятся с относительного пути "engine/shaders/..." → падает при запуске editor из другой папки. Решаем: embed blit-шейдеров в libengine (как sdf_wgsl) или резолв пути рядом с .so.
- [ ] T8 VK SPIR-V: load-time зависимость от VK_SPV_DIR (пути сборки). Минимально: понятная ошибка если spv нет; лучше — embed .spv в .so.
- [ ] T9 Функциональный дубль FNV-хеша в mkdemo.sh (Python) vs engine/asset.c → сверяемся, добавляем комментарий-ссылку + тест-проверку индекса демо движком (asset_test).

### P2 — гигиена
- [ ] T10 main.cpp: extern-объявления внутри блока функции → через panels.h.
- [ ] T11 Магические размеры 1280x720 в viewport.cpp → одна константа Viewport.
- [ ] T12 Прогнать доступные тесты (comp_test/mat4_test/scene_test/asset_test — CPU-only) после правок. ЧАСТИЧНО КРУГ 2: CPU-тесты прогнаны вручную (gcc, без cmake): MAT4 OK / COMP OK / SCENE OK / ASSET OK. Полный cmake-build невозможен в этом окружении: нет glfw3 dev, Vulkan lib, glslc, сети для FetchContent(Dawn). Полная сборка .so+editor — только на машине пользователя (tools/editor.sh).

### Круг 2 — новые задачи (из раздела ЗАМЕРЫ/КРУГ 2 ниже, продублированы здесь для порядка)
- [ ] T13 env-кэш: getenv("VOX_DAYT") x3 каждый кадр → кэшировать в EdConfig при initDomain (расширение T5).
- [ ] T14 Резолв путей шейдеров от каталога .so (dladdr), а не от CWD — чинит и редактор (blit), и GL-ядра движка (N5).
- [ ] T15 Понятная ошибка при отсутствии SPV/VK-библиотек вместо чёрного окна (vk_core loadSpv, N3).

## ЗАМЕРЫ (что нашёл при повторной карте, против прошлого списка)
- Двойной системы сборки НЕТ (только CMake) — прошлый пункт снят.
- Парсинга "key=value" в ABI нет — снят; реальная боль = магические int домена/бэкенда (T3/T4).
- ENV-разброшенность реальна: VOX_* в движке (скриншоты/камера), ED_* в редакторе, VOX_DAYT в рантайме кадра.

## КРУГ 2 — ревизия выполненных T1–T4 + новые находки (проверено по коду, не на глаз)

Подтверждено исправленным: T1, T2, T3, T4 (детали напротив галочек).

НОВНЫЕ находки круга 2:
- N1 viewport.cpp:76,120,145,147,210-211,338-340 — размер вьюпорта 1280x720 зашит ~10 местами (это T11, но теперь с точными строками). Один constexpr Viewport::kW/kH.
- N2 viewport.cpp:132-133 — относительный путь "engine/shaders/blit/*.glsl": работает только если editor запущен из корня репозитория (T7 подтверждён, строки найдены).
- N3 vk_core.c:18-19,115 — fallback VK_SPV_DIR="spv_vk" относительно CWD; молча loadSpv-fail → чёрное окно без внятной ошибки (T8 подтверждён).
- N4 main.cpp:342-345 — extern-объявления (ui_select/ui_world/sc_list/sc_name) внутри тела функции вместо include panels.h (T10 подтверждён, строки найдены).
- N5 rhi_gl_prog("engine/shaders/...") в движке тоже использует относительные пути (gl_core/vk blit) — проблема шире чем редактор: любой вызов из чужого CWD ломает GL-ядра. Резолв через dladdr(&eng_version) → каталог .so.
- N6 Тесты есть (comp/mat4/scene/asset/rhi/phys/engine/dlopen_test.c) и подключены в CMakeLists — но артефактов build/ НЕТ (ни libengine.so, ни editor): предыдущие «правки» никогда не компилировались. До T12 нельзя ставить галочки со словом «проверено сборкой».
- N7 gettenv("VOX_DAYT") вызывается 3 раза каждый кадр (viewport.cpp:402-403) — мелочь, но символ T5: конфиг надо читать один раз при initDomain.

Порядок дальше: T12 (сборка+тесты — валидирует всё ранее «исправленное») → T5 → T11/N1 → T7/N2/N5 → T8/N3 → T10/N4 → T6 → T9.

### Итог круга 2 (выполнено)
- Ревизия T1–T4 по коду: подтверждены, галочки оставлены (см. примечания напротив каждой).
- Тесты CPU-only прогнаны в обход cmake (cmake-конфигурация падает: нет glfw3/vulkan/glslc/сети): mat4_test=OK, comp_test=OK, scene_test=OK, asset_test=OK. Выходные коды 0.
- N6 уточнён: сборку libengine.so и editor здесь проверить нельзя — галочки T1–T4 означают «проверено чтением кода», не «скомпилировано». Финальная верификация — tools/editor.sh на машине с зависимостями.
- Добавлены задачи T13–T15 (env-кэш, резолв путей от .so, явная ошибка VK).

## КРУГ 3–4 — выполненные правки (проверено чтением кода + CPU-тесты; полная сборка — на машине пользователя)
- [x] T5/T13/T18 env-кэш VOX_DAYT: viewport.cpp больше НЕ читает getenv в кадре. Реализовано: Viewport::dayTime() (viewport.h:58-60 setDayTime/dayTime, NAN-sentinel «не читали»), стартовый override один раз в main.cpp (~341-344 ED-day-time), дефолт 242с=полдень. ПРОВЕРЕНО grep: getenv("VOX_DAYT") только в dayTime() (стр. 239) и в main.cpp однократно.
- [x] T7/N2/N5/T14/T16/T17 резолв путей шейдеров от каталога .so: новый API rhi_gl_find (rhi_gl.c:66, dladdr-резолв, комментарий стр. 32). Применён в: viewport.cpp blit (стр. 136-139), mesh_core.c (37-43), sdf_core.c (37-43), gl_core.c через GL_SHADER_DIR+rhi_gl_prog(rhi_gl.c:86-88). Fallback на относительный путь сохранён.
- [x] T8/N3/T15 явная ошибка VK без SPV: vk_core.c — VK_SPV_DIR из env или fallback, dladdr-резолв от .so (стр. 119-143), при отсутствии .spv — fprintf с подсказкой «Build spv (glslc) or set VK_SPV_DIR» (стр. 159) вместо чёрного окна.
- [x] T10/N4 extern-в-теле main.cpp удалены; ui_* из panels.h, sc_* из scene.h (main.cpp:349 комментарий, включены выше).
- [x] T11/N1 магические 1280x720 заменены на Viewport::kW/kH (viewport.cpp:77,121,153,155,218-219,392-395; остались только в комментариях 210,583).
- [x] T6 связь UI-мира с рендером: viewport.cpp использует ui_world()/ui_selected()/sc_get() для влияния выбранной сущности на картинку (стр. 273-276, 461-465, 596-598, 655-682).

### Итог кругов 3–4 (ревизия этой Runde, проверка по факту кода)
- Все перечисленные правки ПРИСУТСТВУЮТ в коде (подтверждено grep/чтением, строки указаны выше).
- CPU-тесты перепрогнаны ЗАНОВО этим кругом: mat4=OK (нужен -lm!), comp=OK, scene=OK, asset=OK, phys=OK. Выходные коды 0.
- Полная сборка libengine.so/editor по-прежнему невозможна в этом окружении (нет GLFW/Vulkan/glslc/сети) — отметка N6 актуальна.
- T9 частично: хеши СИНХРОНИЗИРОВАНЫ по константам (mkdemo.sh:34-37 fnv = asset.c:9-13: offset FNV-1a 64-bit, prime 1099511628211), в mkdemo.sh есть комментарий-ссылка на engine/asset.c (стр. 29). НО: отдельного теста сверки индекса демо движком нет → T9 остаётся незакрытым (нужен asset_test-кейс: захэшировать тайл demо-депо и сверить имя файла).
- Новые находки этого круга:
  - N8 dlopen_test.c:16-17 — список путей libengine.so хардкод (./libengine.so, build/, engine/build/, /tmp/opencode/); рабочий вариант зависит от CWD запуска ctest → хрупко. Дёшево: добавить RWEnv/path из CMake binary dir или оставить (низкий приоритет).
  - N9 mat4_test.c требует -lm (undefined tanf при линковке без math) — в CMake это есть (m), но ручная компиляция по инструкции в MAP ломается; зафиксировать команду с -lm.
  - N10 SkyLUT/WebGPU: webgpu_core.c и sky_lut.c требуют Dawn/FetchContent — вне локальной проверки; риски описаны ранее, задач не добавляем (зависимость от среды).

### Очередь (по порядку)
1. T9 — тест-кейс сверки FNV-индекса (mkdemo.sh vs asset.c) внутри asset_test.c.
2. T12-финал — прогон ВСЕХ тестов (engine/rhi/engine/dlopen_test) на машине пользователя через cmake/tools/editor.sh.
3. N8 — стабилизация путей dlopen_test (опционально, P2).
После закрытия T9 и подтверждения сборки пользователем — план исправлений считается выполненным; остаётся только P2-гигиена без известных открытых дефектов.
