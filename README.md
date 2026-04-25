# FlottyWM

Тайлинговый Wayland compositor на базе **wlroots**. Простой, лёгкий и быстрый.

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

## Конфигурация

Конфиг располагается по пути `~/.config/flottywm/config`.

Пример минимального конфига:

```ini
gaps 8
outer_gaps 8
mfact 0.55

$term alacritty
$launcher fuzzel
$mod Alt

bind $mod+Return exec $term
bind $mod+d exec $launcher
bind $mod+Escape exit
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
bind $mod+Shift+h moveprev
bind $mod+Shift+l movenext

bind $mod+Shift+Left resizeleft
bind $mod+Shift+Right resizeright
bind $mod+Shift+Up resizeup
bind $mod+Shift+Down resizedown
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
| `exec <программа>` | Запустить программу |
| `exit` | Завершить compositor |
| `workspace <N>` | Переключиться на воркспейс N (1–10) |
| `movetoworkspace <N>` | Переместить активное окно на воркспейс N |
| `focusprev` / `focusnext` | Фокус на предыдущее / следующее окно |
| `moveprev` / `movenext` | Поменять активное окно местами |
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
