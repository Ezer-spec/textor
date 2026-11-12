# Textor

Textor - лёгкий, быстрый редактор кода с Metal/GLX-ускорением, подсветкой синтаксиса, sidebar, вкладками, консолью и поддержкой аддонов.

![App Demo](video/2026-07-22-113901_hyprshot.png)
![App Demo](video/2026-07-22-113920_hyprshot.png)

---

## Установка и сборка

### Зависимости

| Дистрибутив | Команда |
|---|---|
| Arch / Artix | `pacman -Syyu xorg cairo pango lib32-pango mesa` |
| Debian / Ubuntu | `apt install libx11-dev libcairo2-dev libpango1.0-dev libgl1-mesa-dev` |
| NixOS | используйте `flake.nix` |

**Компилятор:** Clang или GCC с поддержкой C++17  
**Инструменты:** [Mekanism](https://codeberg.org/Ezerit/mekanism.git) (система сборки)

### Шаги сборки

```bash
# 1. Клонируйте репозиторий
git clone https://codeberg.org/Ezerit/textor.git
cd textor

# 2. Установите и соберите Mekanism
git clone https://codeberg.org/Ezerit/mekanism.git
cd mekanism && ./clang++ -O3 -std=c++17 -D_XOPEN_SOURCE=700 -Wall -I. mekanism.cxx -o build/mekanism
cd ..

# 3. Соберите Textor
./mekanism base

# 4. Запустите
./build/textor
```

**NixOS:**
```bash
nix build
./result/bin/textor
```

---

## Архитектура кода

Textor написан на C++17 как единый header-only проект. Все модули - `.hxx` файлы.

```
textor/
+-- base.cxx                  # Точка входа: X11/macOS mainloop, GPU blit
+-- BUILD.mek                 # Файл сборки (Mekanism)
|
+-- ui/
|   +-- editor.hxx            # * Главный объект Editor + draw_ui()
|   +-- code_editor.hxx       # CodeEditor: буфер (Rope), курсор, подсветка, рисование
|   +-- scroll_bar.hxx        # ScrollBar: вертикальный и горизонтальный скроллбары
|   +-- sidebar.hxx           # Sidebar: файловое дерево
|   +-- settings_tab.hxx      # SettingsTab: вкладка настроек
|   +-- settings_modal.hxx    # SettingsModal: модальное окно (устаревшее)
|   +-- console_panel.hxx     # ConsolePanel: встроенная консоль
|   +-- file_picker.hxx       # FilePicker: диалог выбора файла
|   +-- animations.hxx        # AnimController, ThemeTween: анимации
|   +-- autocomplete.hxx      # Автодополнение
|   +-- adco.hxx              # Движок языка Adco (темы и аддоны)
|   +-- addon_loader.hxx      # Загрузчик .adco файлов (языки + темы)
|   +-- shortcut_manager.hxx  # Управление шорткатами (shortcuts.conf)
|   +-- bar_button.hxx        # BarIcon: кнопки activity bar
|   +-- button.hxx            # TextButton: кнопка сохранения
|   +-- rope.hxx                # Rope: структура данных буфера текста (чистый C++)
|   +-- globals.hxx           # Глобальные переменные (g_animations_enabled и т.д.)
|   +-- platform.hxx          # Платформо-зависимый код
|   +-- icons.hxx             # SVG-иконки
|   +-- splash.hxx            # Заставка при запуске
|   |
|   +-- draw_helpers.hxx      # [NEW] Вспомогательные Cairo-функции (draw_rounded_rect_ed)
|   +-- renderer.hxx          # [NEW] Документация конвейера рендеринга
|   +-- tab_manager.hxx       # [NEW] Документация системы вкладок
|   +-- input_handler.hxx     # [NEW] Документация обработки ввода
|
+-- color/
|   +-- themes.hxx            # ThemeVariant, ThemeTween: темы оформления
|
+-- metal/
|   +-- blit.metal            # Metal shader (macOS GPU blit)
|
+-- themes/                   # .adco файлы тем
+-- addons/                   # .adco файлы аддонов (поддержка языков)
+-- shortcuts.conf            # Настройки шорткатов
+-- settings.conf             # Настройки редактора
```

### Как добавить новый элемент интерфейса

1. **Объявите данные** в `struct Editor` (editor.hxx)
2. **Нарисуйте** в `draw_ui()` в правильном Z-порядке (последний = поверх)
3. **Добавьте hit-test** в `handle_mouse_click()` и `handle_mouse_move()`
4. Смотрите `ui/renderer.hxx` и `ui/input_handler.hxx` для документации

---

## Конвейер рендеринга

```
base.cxx mainloop
    |
    +-- X11 events -> Editor::handle_*()
    |
    +-- draw_ui(cr, surf, editor)        <- CPU: Cairo на s_frame_buf
            |
            +-- activity bar
            +-- tab bar
            +-- gutter (номера строк)
            +-- code area -> CodeEditor::draw_text_area()
            +-- find bar overlay
            +-- scrollbars
            +-- console
            +-- sidebar overlay (поверх всего)
            +-- addons panel
                    |
                    v (GPU mode)
            gpu_blit() -> glTexSubImage2D (только изменённая полоса)
                    |
                    +-- GL overlay: выделение через OpenGL
```

**Оптимизации производительности:**
- `dirty_rect` - частичное обновление GPU текстуры (только изменённые строки)
- `SmartRender` - рисуются только видимые строки (`start_ln..end_ln`)
- `sleep tiers` - при idle > 3с: 2 fps, почти 0% CPU
- GL overlay - выделение рисуется через OpenGL, минуя Cairo

---

## Интерфейс

### Основные области

| Область | Описание |
|---|---|
| **Activity Bar** | Вертикальная панель слева: sidebar, аддоны, консоль, настройки, выход |
| **Tab Bar** | Горизонтальная панель вкладок. Прокручивается колесом мыши |
| **Sidebar** | Файловое дерево (плавающий overlay). Ctrl+B или кнопка |
| **Gutter** | Номера строк слева от кода |
| **Text Area** | Основная область редактирования |
| **Console** | Встроенная кон
соль снизу. Ctrl+\` или кнопка |
| **Find Bar** | Поиск по файлу. Ctrl+F |

### Шорткаты (по умолчанию)

| Действие | Шорткат |
|---|---|
| Открыть файл | Ctrl+O |
| Новый файл | Ctrl+N |
| Сохранить | Ctrl+S |
| Закрыть вкладку | Ctrl+W |
| Следующая вкладка | Ctrl+Tab |
| Предыдущая вкладка | Ctrl+Shift+Tab |
| Настройки | Ctrl+, |
| Sidebar | Ctrl+B |
| Консоль | Ctrl+\` |
| Поиск | Ctrl+F |
| К курсору | Ctrl+L |
| Выход | Ctrl+Q |

Шорткаты можно переназначить в `shortcuts.conf` или через вкладку Settings.

---

## Темы и настройки

Темы пишутся на языке Adco (файлы `.adco`) в папках `themes/` или `~/.config/textor/themes/`.
См. [docs/ADCO.md](docs/ADCO.md) и `themes/example.adco`.
Готовые темы в `themes/`: Midnight Ocean, Amber Forge, Nord Frost, Rose Dusk, Example Amber.
В палитре команд (Ctrl+Shift+P) введите название темы: выбранная тема сразу показывается, Enter оставляет её, Esc возвращает прежнюю.

Настройки сохраняются в `~/.config/textor/settings.conf`.

**Параметры производительности** (Settings -> Performance):
- **Animations** - плавные анимации переходов
- **Rounded corners** - скругления элементов
- **Text optimization** - экономия CPU при рендеринге текста
- **Line scroll** - прокрутка по строкам вместо пикселей
- **Smart render** - рисовать только видимые строки

---

## Аддоны

Аддоны - файлы `.adco` в `addons/` или `~/.config/textor/addons/`.
Аддон добавляет поддержку языка (подсветка, комментарии, автодополнение, иконка файла) и может содержать темы.
C, C++, D, Rust, Python, Shell, Lua, Kotlin, Mekanism и конфиги подсвечиваются самим редактором; Adco и остальные языки подключаются аддонами.

Язык описан в [docs/ADCO.md](docs/ADCO.md); небольшой пример - `addons/lua.adco`.

---

## Изменения в этой версии

### ! Исправленные баги
- Выделение текста не остаётся при переходе на вкладку настроек
- Выделение не наслаивается на tab bar при прокрутке
- Ползунок скроллбара не выходит за границы трека при ручной прокрутке
- **[NEW]** Ctrl+V теперь работает в поисковой строке (find bar)
- **[NEW]** Find bar (Ctrl+F) теперь выдвигается сверху с анимацией и сдвигает код вниз, не перекрывая скроллбар
- **[NEW]** Панель аддонов теперь закрыта при запуске редактора

### ! Новые функции
- **Маркеры совпадений на скроллбаре** - при поиске (Ctrl+F) или выделении слова на треке скроллбара появляются кружки, показывающие где есть совпадения. Активное совпадение подсвечивается ярче
- **Анимация find bar** - плавное появление/скрытие поисковой строки
- **Ctrl+F с выделением** - если перед нажатием Ctrl+F выделен текст, он сразу становится запросом

### ! Оптимизация памяти
- **Кеш буфера** - `get_buffer_str()` кеширует `buffer.to_string()` и инвалидирует при каждой правке. Поиск, undo, autocomplete больше не копируют весь файл каждый раз
- **deque для undo-стека** - `erase(begin())` теперь O(1) вместо O(n)
- **Адаптивный MAX_UNDO** - для файлов > 100KB лимит undo снижается с 200 до 50 записей (экономия сотен MB для больших файлов)
- **find_bar.clear()** - вектор совпадений освобождается через swap-trick немедленно

### ! Рефакторинг
- Новые модули с документацией: `draw_helpers.hxx`, `renderer.hxx`, `tab_manager.hxx`, `input_handler.hxx`
- Все основные структуры и методы документированы на русском
- Ускорение рендера: dirty-rect partial GPU upload (только изменённые строки)
- Sleep tiers: idle > 3с -> 2fps, почти 0% CPU

---

## Решение проблем

**Курсор <<телепортирует>> вверх** при редактировании внизу файла:  
> `ensure_cursor_visible` должна использовать `abs_cy = line * line_h + pos.y`

**Выделение остаётся на настройках:**  
> Исправлено в этой версии: `gpu_sel_active` сбрасывается в `switch_to_tab()` и при рисовании settings tab

**Скроллбар вылезает за границы:**  
> Исправлено в `scroll_bar.hxx`: thumb теперь зажат через `std::clamp(thumb_raw, 0.0, track_h - thumb_h)`

**Как участвовать в разработке:**  
Присылайте патчи в формате `git diff` как pull request на Codeberg.  
В описании PR указывайте: цель изменения, затронутые файлы, поведение до/после.
