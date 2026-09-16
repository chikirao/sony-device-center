# Dev workflow на Windows — как собирать, запускать и проверять

Документ для разработки на этой машине. Отличия от веб-разработки в двух
вещах: **нет hot reload** (изменил → скомпилировал → запустил exe) и **нужен
тулчейн** (компилятор + Qt), который ставится один раз.

## Чем отличается от веба (коротко)

| Веб | Здесь |
|---|---|
| `npm install` | Visual Studio 2022 + Qt 6 (один раз) |
| `npm run dev`, изменения подхватываются сами | `cmake --build build` → запуск `.exe`. Инкрементальная сборка после правки одного файла — 5–30 секунд, первая полная — 5–10 минут |
| Открыть localhost в браузере | Запустить `build\apps\device-center\sony-device-center.exe` |
| DevTools / console.log | `qDebug()` / `std::cerr` → в консоль, из которой запущен exe; QML-ошибки туда же |
| Jest / Vitest | CTest (`ctest --test-dir build`) — готовые тесты протокола, транспорта, IPC, UI-контроллера |
| Деплой | Установщик собирает GitHub Actions (`release.yml`), локально ставим ничего не нужно — exe запускается прямо из `build/` |

Установленную через MSI версию трогать не надо: dev-сборка живёт в `build/`,
они не мешают друг другу. Только не запускай обе одновременно — обе хотят
Bluetooth-соединение с наушниками.

## 1. Установка тулчейна (один раз)

Что уже есть:
- Visual Studio 2022 Community, MSVC 14.35 —
  `C:\Program Files\Microsoft Visual Studio\2022\Community`
- CMake и Ninja в составе VS —
  `...\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\{CMake,Ninja}`

Чего нет — **Qt 6**. Ставим тем же способом, что и CI (`jurplel/install-qt-action`
использует `aqtinstall`), без аккаунта Qt:

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.10.0 win64_msvc2022_64 --outputdir D:\Qt
```

Получится `D:\Qt\6.10.0\msvc2022_64`. Базовая установка включает всё, что нужно
приложению (Core, Gui, Qml, Quick, QuickControls2, Shapes). ~1.5 ГБ.

Если `6.10.0` не находится — посмотреть доступные:
`aqt list-qt windows desktop`.

## 2. Сборка

Всё обёрнуто в `scripts\win-dev.ps1` — он сам поднимает окружение MSVC
(через `vcvars64.bat`), добавляет Qt в PATH и знает про особенности ниже.
Запускать из обычного PowerShell в корне репозитория:

```powershell
.\scripts\win-dev.ps1 configure   # один раз, и после правок CMakeLists / смены ветки
.\scripts\win-dev.ps1 build       # инкрементальная сборка
.\scripts\win-dev.ps1 test        # ctest (можно передать -R <regex>)
.\scripts\win-dev.ps1 run         # сборка + windeployqt + запуск GUI
.\scripts\win-dev.ps1 ctl info    # сборка + sonyctl с аргументами
```

Что скрипт делает за кадром (на случай ручного запуска):

- `configure` = `git submodule update --init` + `cmake -B build -G Ninja
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt.10.0\msvc2022_64
  -DBUILD_TESTING=ON -DSONY_REQUIRE_QT=ON`. Сабмодуль `Client/imgui` нужен
  legacy-клиенту, без него конфигурация падает.
- `build` сначала собирает **один** объект `qrc_qml.cpp.obj` в один поток, и
  только потом всё остальное параллельно. В `qml.qrc` упакован 21 МБ
  PNG-картинок устройств → 110 МБ C++-файл; при параллельной компиляции MSVC
  падает с `C1060: out of heap space`. (Постоянный фикс — в roadmap.)
- Окружение MSVC берётся из `vcvars64.bat`. `Launch-VsDevShell.ps1` на этой
  машине не находит `vswhere` и не добавляет `rc.exe` — не использовать.

Ninja сам понимает, какие файлы изменились. Правка `.cpp` — пересобирается
один файл и линкуется exe. Правка `.h` — пересобирается всё, что его
включает. Правка `.qml` — QML упакован в exe через `qml.qrc`, поэтому тоже
нужна пересборка (пересобирается тот самый тяжёлый `qrc_qml.cpp`, ~1 мин).

`Release` вместо `Debug` — намеренно: Debug-сборка Qt-приложения заметно
медленнее стартует, а отладчиком мы почти не пользуемся. Когда понадобится
пошаговая отладка — отдельная папка `build-debug` с `-DCMAKE_BUILD_TYPE=Debug`.

## 3. Запуск

Exe нужны Qt DLL. Два варианта:

**A. Добавить Qt в PATH на время сессии** (быстро, для разработки):

```powershell
$env:PATH = "D:\Qt\6.10.0\msvc2022_64\bin;$env:PATH"
.\build\apps\device-center\sony-device-center.exe
```

**B. windeployqt** — копирует DLL рядом с exe, получается самодостаточная
папка как у установленной версии (это же делает `cmake --install`):

```powershell
D:\Qt\6.10.0\msvc2022_64\bin\windeployqt.exe --qmldir apps\device-center\qml build\apps\device-center\sony-device-center.exe
```

После этого exe запускается двойным кликом из Explorer. Делать заново не
нужно, пока не меняется набор Qt-модулей.

Приложение собрано как GUI-subsystem (`WIN32_EXECUTABLE ON`), поэтому
консоли у него нет. Чтобы видеть `qDebug`/`std::cerr` — запускать из
терминала: вывод пойдёт туда. Если не идёт — временно
`$env:QT_LOGGING_RULES="*.debug=true"` или запускать `sonyctl -v` для
диагностики протокола.

CLI собирается рядом:

```powershell
.\build\apps\sonyctl\sonyctl.exe info
.\build\apps\sonyctl\sonyctl.exe -v battery
```

На Windows `sonyd` (демон) не работает — IPC не реализован
(`libs/sony-core/src/IpcServer.cpp`). GUI и `sonyctl` каждый открывают
Bluetooth напрямую, поэтому **одновременно они не работают**: закрыть GUI
перед `sonyctl`, и наоборот. Это в roadmap, фаза 5.

## 4. Тесты

```powershell
.\scripts\win-dev.ps1 test
```

Qt-тестам нужны Qt DLL в PATH — скрипт это делает. Голый `ctest` без PATH
валит `device-controller-worker-integration` с кодом `0xc0000135` (DLL not
found) — это не баг теста.

Что покрыто: кодек фреймов, протоколы V1/V2 на фейковом транспорте (с
литеральными байтами запросов/ответов), диспетчер уведомлений, IPC (на
Windows часть пропускается), Qt-контроллер. Наушники для тестов не нужны.

Прогон одного набора: `.\build\tests\sony-protocol-tests.exe`.

Правило для протокольных фич: сначала снять реальные байты с XM5
(`sonyctl -v <команда>` печатает hex-дамп фреймов), положить их в тест как
фикстуру, потом писать реализацию под этот тест.

## 5. Цикл разработки одной фичи

1. Ветка: `git checkout -b feat/tray-icon`.
2. Правим код (Claude Code делает это в редакторе / через инструменты).
3. `cmake --build build --parallel` — компилируется? Ошибки компилятора
   читаем как ошибки TypeScript, они точные.
4. `ctest --test-dir build` — ничего не сломали?
5. Запускаем exe, тыкаем руками с наушниками.
   Для протокольных фич — сначала `sonyctl`, там проще смотреть байты.
6. Коммит, PR в свой форк.

Пункты 3–4 Claude Code может гонять сам из чата; пункт 5 (GUI с реальными
наушниками) — ты, потому что нужен взгляд на экран и уши. Скриншот окна
можно кинуть в чат.

## 6. Проверка UI без наушников

```powershell
.\scripts\win-dev.ps1 run --simulated
```

GUI поднимает встроенный симулятор WH-1000XM5 прямо в процессе (тот же, что у
`sonyd --simulated`, код в `libs/sony-core/src/SimulatedDevice.cpp`). Он
отвечает на запросы, применяет SET-команды и шлёт уведомления, так что
переключатели и слайдеры ведут себя как с настоящими наушниками. Для работы
над UI, треем и темами — основной режим; наушники нужны только для
протокольных фич.

## 7. Установщик

Локально не нужен. `release.yml` в GitHub Actions при пуше тега `v*`
собирает exe + NSIS-установщик и выкладывает в Releases форка. Когда
захочется «поставить себе как нормальное приложение» — `cmake --install build
--prefix C:\Apps\SonyDeviceCenter` даст готовую папку с DLL, без установщика.

## Частые проблемы

- **`Qt6Config.cmake not found`** — не передан `CMAKE_PREFIX_PATH` или
  другая версия/путь Qt.
- **`cl` / `rc` не найден** — запускать через `scripts\win-dev.ps1`, не через Launch-VsDevShell.
- **`C1060: compiler is out of heap space`** — сборка `qrc_qml.cpp` параллельно; скрипт собирает его отдельно.
- **`Cannot find source file: imgui/imgui.cpp`** — не скачан сабмодуль: `git submodule update --init --recursive`.
- **Exe запускается и сразу закрывается** — не хватает Qt DLL; п. 3.
- **Белое/пустое окно, в консоли `module "QtQuick.Shapes" is not installed`** —
  windeployqt запущен без `--qmldir`, или Qt в PATH стоит не первым.
- **`Bluetooth: connection refused / device busy`** — открыт второй экземпляр
  (установленная версия, `sonyctl`, или Sound Connect на телефоне держит
  соединение). Закрыть лишнее.
- **После смены ветки странные ошибки сборки** — `cmake -B build ...` заново;
  в крайнем случае удалить `build/`.
