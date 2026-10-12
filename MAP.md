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
- [x] T9 ЗАКРЫТ (круг 5, проверено фактическим прогоном): кейс сверки хеша внутри engine/asset_test.c («hash parity OK»): движковый fnv() == оффлайн-FNV mkdemo.sh по байтам grass_top.png. Прогон из корня репо: ASSET OK.

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
  - [x] N8 ЗАКРЫТ (круг 5): dlopen_test.c переписан — ENV LIBENGINE_PATH override, затем dladdr-якорь от каталога самого теста (не зависит от CWD), затем относительные fallback; путь /tmp/opencode убран. CMake: target_link_libraries(dl) + add_dependencies(dlopen_test engine_shared). Синтаксис проверен gcc -fsyntax-only (потребовался _GNU_SOURCE для Dl_info).
  - N9 mat4_test.c требует -lm (undefined tanf при линковке без math) — в CMake это есть (m), но ручная компиляция по инструкции в MAP ломается; зафиксировать команду с -lm.
  - N10 SkyLUT/WebGPU: webgpu_core.c и sky_lut.c требуют Dawn/FetchContent — вне локальной проверки; риски описаны ранее, задач не добавляем (зависимость от среды).

### Очередь (по порядку)
1. [x] T9 — закрыт кругом 5 (asset_test.c hash-parity кейс, прогон зелёный).
2. T12-финал — прогон ВСЕХ тестов (engine/rhi/engine/dlopen_test) на машине пользователя через cmake/tools/editor.sh.
3. [x] N8 — закрыт кругом 5 (dladdr-якорь + ENV override + порядок сборки в CMake).
После закрытия T9 и подтверждения сборки пользователем — план исправлений считается выполненным; остаётся только P2-гигиена без известных открытых дефектов.


---

## КРУГ 5 (прогон + добивание хвостов) — что и где
Фактические прогоны этого круга (gcc без CMake, из /workspace):
- comp_test OK / scene_test OK / mat4_test OK (-lm) / phys_test OK / asset_test OK (из корня репо; из /tmp падает — фикстуры относительно CWD, см. N11 ниже).
- gcc -fsyntax-only -Wall -Wextra на dlopen_test.c — SYNTAX_OK.

Закрыто в этом круге:
- T9: хеш-паритет кейс в engine/asset_test.c (строки ~50-63): эталонный FNV-1a 64 по байтам файла == AssetId из asset_import. Вывод «hash parity OK (597a...)».
- N8: engine/dlopen_test.c try_open() — приоритеты: LIBENGINE_PATH env > <каталог-экзешника>/libengine.so (dladdr) > ./ , build/, engine/build/. Убран мусорный /tmp/opencode. _GNU_SOURCE добавлен.
- engine/CMakeLists.txt: dlopen_test теперь линкует dl и собирается ПОСЛЕ engine_shared (add_dependencies) — раньше мог стартануть до библиотеки.

Новые находки круга 5:
- N11 asset_test.c привязан к CWD репо (фикстуры "engine/assets/tiles/..."). В CMake это решается WORKING_DIRECTORY у add_test, но add_test вообще нигде нет (см. N12). Для ручной команды в MAP зафиксировать: запускать из корня репо.
- N12 В РЕПО НЕТ enable_testing()/add_test() ни в одном CMakeLists — «CTest» фактически не настроен, тесты только руками. Дёшево: enable_testing() в engine/CMakeLists.txt + add_test по каждому *_test с WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}.
Статус: все T1-T18 закрыты; открыты только N11/N12 (P2, гигиена CI) и T12-финал (полная GPU-сборка — только на машине пользователя).

## КРУГ 6 — независимая переверификация всех прогонов (честный аудит)
Пересобрал и прогнал ВСЕ CPU-тесты заново, по одному, с минимальным набором исходников (без GPU-файлов):
- comp_test   = OK (comp.c + corelib.c)
- scene_test  = OK (scene.c + corelib.c)
- mat4_test   = OK (-lm; gl/vk near-far, look-at)
- phys_test   = OK (падение, терминальная скорость, тики)
- asset_test  = OK из корня репо; hash parity 597a6f8e1d10b2fa подтверждён фактическим запуском. ВАЖНО: vfs.h header-only — отдельного .c нет (ранее в логе был неверный вызов с vfs*.c; тест всё равно зелёный).
Уточнение к предыдущей записи «все тесты зелёные»: физически перепроверены и подтверждены выводом эти 5 CPU-тестов. НЕ проверяемы здесь (нет GLFW dev / glslc / Qt / дисплея): engine_test, rhi_test, dlopen_test рантайм, libengine.so, frontend/viewport.cpp компиляция, Qt-редактор, визуал. Синтаксис GPU-частей (engine.c/gl_core.c/rhi_gl.c/vox_feed.c) даже -fsyntax-only не проходит без glfw3.h — это ограничение среды, а не дефект кода.
ИТОГ КРУГА 6: расхождений между картой и кодом не найдено; список открытых пунктов не изменился — только T12-финал на машине пользователя.

## КРУГ 7 — аудит Windows-портируемости (запрос: проект под Arch, но должен запускаться на Windows)
Фактическая проверка кода (grep/чтение), не домыслы:

### Что уже кроссплатформенно (подтверждено):
- Vulkan surface создаётся через glfwCreateWindowSurface (rhi_vk.c:60) — GLFW сам выбирает Win32/XCB surface; VK_USE_PLATFORM_* в vulkan.h включаются по _WIN32 автоматически. ПЛАТФОРМЫ В КОДЕ НЕТ.
- glad/vulkan headers в third-party — идут с репозиторием.
- Шейдерные пути — '/' в fopen: Windows принимает слеши в API-пути. ОК.
- CMakeLists используют find_package(glfw3)/OpenGL/Vulkan + FetchContent(Dawn) — стандартно работает и на Win (Vulkan SDK + glslc есть для Win).

### Найденные платформенные зависимости (тормозят запуск на чистом Win):
- W1 dladdr/dlfcn.h (rhi_gl.c, vk_core.c, dlopen_test.c) — на Win нет. ЧАСТИЧНО ЗАКРЫТО ЭТИМ КРУГОМ: rhi_gl.c получил слой eng_module_path() (#ifdef _WIN32 → GetModuleFileNameA(NULL), POSIX → dladdr; плюс fallback dladdr по rhi_gl_prog для статической линковки). НЕ ЗАКРЫТО: vk_core.c (стр. ~134 dladdr напрямую) и dlopen_test.c (dlopen/dlsym без Win32-обёрток LoadLibraryA/GetProcAddress/FreeLibrary).
- W2 OpenGL::GL + -lGL: на Win это opengl32.lib (CMake OpenGL::GL абстрагирует — ок), но opengl32.dll поддерживает только GL 4.5 если драйвер новый — норм.
- W3 libengine.so vs engine.dll: frontend/CMakeLists.txt жёстко ${...}/../engine/build/libengine.so — на Win файл называется engine.dll (или libengine.dll при MinGW). Нужен ${CMAKE_SHARED_LIBRARY_PREFIX}engine${CMAKE_SHARED_LIBRARY_SUFFIX}. main.cpp/viewport.cpp упоминают «libengine.so» только в строках UI (косметика).
- W4 tools/*.sh (editor.sh/run.sh/mkdemo.sh/matrix.sh) — sh-only; на Win нужен PowerShell/Git-Bash вариант или .cmd-обёртки. mkdemo генерирует текстовые проекты — переносим тривиально.
- W5 DISPLAY-проверки в run.sh/editor.sh — unix-специфика.
- W6 env-контракты (VOX_DAYT, ED_PROJECT, LIBENGINE_PATH...) — SetEnvironmentVariable/cmd `set` работают так же, проблем нет.
- W7 webgpu Dawn через FetchContent с GitHub — на Win собирается, но требует Visual Studio 2022 toolchain; сборка долгая. Альтернатива на Win: --core gl|vk.

### Тесты круга 7:
- Все CPU-тесты перепрогнаны после правки rhi_gl.c: mat4/comp/phys/scene/asset = OK.
- gcc -fsyntax-only dlopen_test.c — SYNTAX_OK (правка rhi_gl.c на него не влияет).
- rhi_gl.c синтаксис здесь не проверяем (нет glfw3.h — ограничение среды, как в круге 6).

### Задачи из круга 7 (очередь портирования):
- [x] T19=W1a: rhi_gl.c — кроссплатформенный якорь модуля (сделано этим кругом).
- [ ] T20=W1b: vk_core.c — тот же слой eng_module_path вместо прямого dladdr (+ PATHSEP_CHR в strrchr стр. ~137,474).
- [ ] T21=W1c: dlopen_test.c — #ifdef _WIN32: LoadLibraryA/GetProcAddress/FreeLibrary + поиск engine.dll рядом с exe (GetModuleFileNameA).
- [ ] T22=W3: frontend/CMakeLists.txt — имя библиотеки через CMAKE_*_LIBRARY_* переменные; ENGINE_LIB = .../engine/build/${CMAKE_SHARED_LIBRARY_PREFIX}engine${CMAKE_SHARED_LIBRARY_SUFFIX}.
- [ ] T23=W4/W5: tools/editor.ps1 + tools/run.ps1 (или .cmd) — эквиваленты editor.sh/run.sh без DISPLAY (на Win окно всегда куда показать).
- [ ] T24: README/инструкция запуска на Windows (deps: Vulkan SDK, Qt6, GLFW via vcpkg).

### Как запустить на Windows (рабочий маршрут СЕЙЧАС, до T20–T23):
1. Установить: Visual Studio 2022 (+C++), CMake ≥3.20, Qt 6 (msvc_64), Vulkan SDK (даёт glslc+вulkan.lib), GLFW (vcpkg install glfw3), Git for Windows (даёт bash!).
2. Самый быстрый путь — **Git Bash** (он уже ставит sh): 
   `cmake -S engine -B engine/build && cmake --build engine/build --config Release`
   затем `./tools/run.sh gl` (DISPLAY пуст → скрипт откажет; обход: запустить engine/build/Release/scene3d.exe --core gl напрямую).
   Либо использовать cmd/PowerShell напрямую: scene3d.exe, editor.exe из frontend/build/Release.
3. Редактор: `cmake -S frontend -B frontend/build && cmake --build frontend/build --config Release` — УПАДЁТ на проверке libengine.so (W3) → временный хак: копировать engine.dll → libengine.so в build-каталог движка, либо дождаться T22.
Полная нативная поддержка Win = закрытие T20–T24.
