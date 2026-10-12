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
- [ ] T1 `rc_destroy` не освобождает сам RenderCore (утечка каждого ядра; Viewport::initDomain при FAILED теряет newRc). Правим corelib.c.
- [ ] T2 Реестр: молчаливый отказ при переполнении (cap 16) и NULL-ах → явный stderr-лог.
- [ ] T3 front/back контракт backend: магические числа 1/2/3 в main.cpp/hub/viewport → константы RC_BACKEND_* из render_api.h (frontend уже его включает).
- [ ] T4 domainForTemplate возвращает сырые 1/2/4/8/16 → константы RC_CAP_*; неизвестный шаблон ≠ воксель тихо (лог в консоль).
- [ ] T5 view-параметры редактора: VOX_DAYT из env внутри renderOnce → поле EdConfig (дефолт без env), env только как CLI override.
- [ ] T6 Разъединить два «scene»: UI-мир ScWorld (panels) не влияет на рендер. Минимум: feed воксельного ядра учитывает трансформ выбранной сущности (гизмо реально дёргает картинку) ИЛИ честная пометка. Проверяем по коду, выбираем дешёвый честный вариант + запись в консоль.

### P1 — сборка/ресурсы
- [ ] T7 Шейдеры blit грузятся с относительного пути "engine/shaders/..." → падает при запуске editor из другой папки. Решаем: embed blit-шейдеров в libengine (как sdf_wgsl) или резолв пути рядом с .so.
- [ ] T8 VK SPIR-V: load-time зависимость от VK_SPV_DIR (пути сборки). Минимально: понятная ошибка если spv нет; лучше — embed .spv в .so.
- [ ] T9 Функциональный дубль FNV-хеша в mkdemo.sh (Python) vs engine/asset.c → сверяемся, добавляем комментарий-ссылку + тест-проверку индекса демо движком (asset_test).

### P2 — гигиена
- [ ] T10 main.cpp: extern-объявления внутри блока функции → через panels.h.
- [ ] T11 Магические размеры 1280x720 в viewport.cpp → одна константа Viewport.
- [ ] T12 Прогнать доступные тесты (comp_test/mat4_test/scene_test/asset_test — CPU-only) после правок.

## ЗАМЕРЫ (что нашёл при повторной карте, против прошлого списка)
- Двойной системы сборки НЕТ (только CMake) — прошлый пункт снят.
- Парсинга "key=value" в ABI нет — снят; реальная боль = магические int домена/бэкенда (T3/T4).
- ENV-разброшенность реальна: VOX_* в движке (скриншоты/камера), ED_* в редакторе, VOX_DAYT в рантайме кадра.
