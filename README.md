# FlottyWM

Тайлинговый Wayland compositor на базе **wlroots**. Простой, лёгкий и быстрый.

## Философия

- **Минимализм** — только то, что нужно для работы. Нет анимаций, blur, теней, скруглений и другого декора.
- **Стабильность** — код простой и предсказуемый. Меньше кода = меньше багов.
- **Оптимизация** — всё ради скорости и низкого потребления ресурсов. Никаких "красивостей" за счёт CPU/GPU.
- **Практичность** — конфиг понятен с первого взгляда, hot-reload без перезапуска, автостарт из коробки.

Если ты ищешь eyecandy с анимациями и эффектами — это не сюда. FlottyWM для тех, кто хочет, чтобы WM просто работал и не мешал.

## Возможности

- **Master-stack layout** — классическое тайлинговое расположение окон
- **Рабочие пространства** — 10 виртуальных десктопов
- **Жёлтая рамка** активного окна (как в Hyprland)
- **Конфиг через переменные** — `$mod`, `$term`, `$launcher` и т.д.
- **Управление клавиатурой** — фокус, перемещение, resize, переключение воркспейсов
- **Layer Shell** — поддержка панелей, обоев и оверлеев (waybar, swaybg и т.п.)

## Зависимости (Debian 13 / Trixie)

```bash
sudo apt update
sudo apt install -y \
    build-essential meson ninja-build pkg-config \
    libwlroots-0.18-dev libwayland-dev wayland-protocols \
    libxkbcommon-dev libc6-dev
```

> Если в репозитории доступен `libwlroots-0.19-dev`, используйте его и обновите `wlroots = dependency('wlroots-0.18')` в `meson.build` соответственно.

## Сборка

```bash
# Клонирование
git clone https://github.com/Raspunt/wayland_compositor.git flottywm
cd flottywm

# Сборка через make (обёртка над meson + ninja)
make build

# Или полная пересборка с нуля
make rebuild
```

После сборки бинарник будет лежать в `./build/flottywm`.

## Запуск

```bash
# Для разработки / nested mode (внутри X11 или другого Wayland compositor)
make run

# Или напрямую
./build/flottywm

# Для запуска из TTY (DRM backend)
# Просто запустите из виртуальной консоли:
./build/flottywm
```

## Screencast (OBS, Discord и т.д.)

FlottyWM поддерживает протоколы `zwlr_screencopy_manager_v1` и `zwlr_export_dmabuf_manager_v1`. Для захвата экрана в OBS, Discord и других приложениях установите:

```bash
sudo apt install pipewire xdg-desktop-portal xdg-desktop-portal-wlr
```

Запустите `pipewire` и `xdg-desktop-portal` (обычно они стартуют автоматически через systemd в пользовательской сессии). После этого в OBS выбирайте источник **PipeWire Screen Capture**.

> **Важно:** убедитесь, что в системе не запущен другой `xdg-desktop-portal` backend (например, `xdg-desktop-portal-gtk` или `xdg-desktop-portal-kde`) одновременно с `xdg-desktop-portal-wlr`, иначе портал может конфликтовать.

## Конфигурация

Конфиг располагается по пути `~/.config/flottywm/config`.

Поддерживаются комментарии через `#` (в начале строки или в конце).

Пример минимального конфига:

```ini
gaps 8
outer_gaps 8
mfact 0.55  # соотношение master/stack

$term alacritty
$launcher fuzzel

# Переменные окружения
# env SDL_VIDEODRIVER wayland
# env MOZ_ENABLE_WAYLAND 1

# Автостарт программ при запуске compositor'а
# exec-once waybar
# exec-once swaybg -i ~/Pictures/wallpaper.jpg -m fill

$mod Alt

bind $mod+Return exec $term
bind $mod+d exec $launcher
bind $mod+Escape exit
bind $mod+Shift+space togglefloating
bind $mod+Shift+c reload
bind $mod+q kill

bind $mod+1 workspace 1
bind $mod+2 workspace 2
bind $mod+3 workspace 3
bind $mod+4 workspace 4
bind $mod+5 workspace 5
bind $mod+6 workspace 6
bind $mod+7 workspace 7
bind $mod+8 workspace 8
bind $mod+9 workspace 9
bind $mod+0 workspace 10

bind $mod+Shift+1 movetoworkspace 1
bind $mod+Shift+2 movetoworkspace 2
bind $mod+Shift+3 movetoworkspace 3
bind $mod+Shift+4 movetoworkspace 4
bind $mod+Shift+5 movetoworkspace 5
bind $mod+Shift+6 movetoworkspace 6
bind $mod+Shift+7 movetoworkspace 7
bind $mod+Shift+8 movetoworkspace 8
bind $mod+Shift+9 movetoworkspace 9
bind $mod+Shift+0 movetoworkspace 10

bind $mod+h focusprev
bind $mod+l focusnext
bind $mod+Left focusleft
bind $mod+Right focusright
bind $mod+Up focusup
bind $mod+Down focusdown
bind $mod+Shift+h moveprev
bind $mod+Shift+l movenext

bind $mod+Ctrl+Left resizeleft
bind $mod+Ctrl+Right resizeright
bind $mod+Ctrl+Up resizeup
bind $mod+Ctrl+Down resizedown
```

### Переменные

Любое имя, начинающееся с `$`, становится переменной конфига:

```ini
$mod Alt
$term alacritty
bind $mod+Return exec $term
```

### Доступные команды

| Команда | Описание |
|---------|----------|
| `env <NAME> <VALUE>` | Задать переменную окружения |
| `exec <программа>` | Запустить программу |
| `exec-once <команда>` | Запустить при старте compositor'а (waybar, swaybg и т.д.) |
| `exit` | Завершить compositor |
| `workspace <N>` | Переключиться на воркспейс N (1–10) |
| `movetoworkspace <N>` | Переместить активное окно на воркспейс N |
| `focusprev` / `focusnext` | Фокус на предыдущее / следующее окно |
| `focusleft` / `focusright` / `focusup` / `focusdown` | Фокус в заданном направлении |
| `moveprev` / `movenext` | Поменять активное окно местами |
| `togglefloating` | Переключить окно в floating / tiled |
| `reload` | Перезагрузить конфиг без перезапуска |
| `kill` | Закрыть активное окно |
| `resizeleft` / `resizeright` | Изменить ширину master-области |
| `resizeup` / `resizedown` | Изменить размер floating окна |

### Настройки

| Параметр | Описание | Значение по умолчанию |
|----------|----------|----------------------|
| `gaps` | Отступы между окнами | `8` |
| `outer_gaps` | Внешние отступы от краёв экрана | `8` |
| `mfact` | Доля экрана для master-области | `0.55` |

## Управление мышью

- **Win + Правая кнопка** — перетаскивание floating окна
- **Клик по окну** — фокус

## Лицензия

MIT
