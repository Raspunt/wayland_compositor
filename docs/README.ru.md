# FlottyWM

Тайлинговый Wayland compositor на базе **wlroots**. Простой, лёгкий и быстрый.

## Философия

- **Минимализм** — только то, что нужно для работы. Нет анимаций, blur, теней, скруглений и другого декора.
- **Стабильность** — код простой и предсказуемый. Меньше кода = меньше багов.
- **Оптимизация** — всё ради скорости и низкого потребления ресурсов.
- **Практичность** — конфиг понятен с первого взгляда, hot-reload без перезапуска, автостарт из коробки.

## Возможности

- **Master-stack layout** — классическое тайлинговое расположение окон
- **10 рабочих пространств**
- **Жёлтая рамка** активного окна, тёмно-серая — неактивного
- **Floating режим** — любое окно можно перевести в floating (`$mod+Shift+space`)
- **Clipboard** — полная поддержка Wayland clipboard, primary selection (средняя кнопка мыши) и внешних утилит (`wl-copy` / `wl-paste`)
- **Screencast** — захват экрана в OBS, Discord и других приложениях через PipeWire
- **Layer Shell** — поддержка панелей, обоев и оверлеев (waybar, swaybg, fuzzel и т.п.)
- **Конфиг через переменные** — `$mod`, `$term`, `$launcher`
- **Hot-reload конфига** — `$mod+Shift+c` перезагружает конфиг без перезапуска
- **Автостарт** — `exec-once` программы при запуске compositor'а
- **Wallpaper из коробки** — дефолтный wallpaper + `swaybg` в конфиге

## Зависимости (Debian 13 / Trixie)

```bash
sudo apt update
sudo apt install -y \
    build-essential meson ninja-build pkg-config \
    libwlroots-0.18-dev libwayland-dev wayland-protocols \
    libxkbcommon-dev libc6-dev
```

> Если в репозитории доступен `libwlroots-0.19-dev`, используйте его и обновите `wlroots = dependency('wlroots-0.18')` в `meson.build`.

## Сборка

```bash
git clone https://github.com/Raspunt/flottyWM.git flottywm
cd flottywm
make build        # или make rebuild для чистой пересборки
```

Бинарник: `./build/flottywm`

## Запуск

```bash
# Nested mode (внутри X11 или другого Wayland compositor)
make run

# Или напрямую
./build/flottywm

# Из TTY (DRM backend)
./build/flottywm
```

## Screencast (OBS, Discord)

FlottyWM поддерживает протоколы `zwlr_screencopy_manager_v1` и `zwlr_export_dmabuf_manager_v1`.

```bash
sudo apt install pipewire xdg-desktop-portal xdg-desktop-portal-wlr
```

Запустите `pipewire` и `xdg-desktop-portal` (обычно через systemd user session). В OBS выбирайте **PipeWire Screen Capture**.

> **Важно:** не запускайте другие `xdg-desktop-portal` backend'ы (`-gtk`, `-kde`) одновременно с `-wlr`.

## Конфигурация

Конфиг располагается по пути `~/.config/flottywm/config`. При первом запуске compositor автоматически создаёт директорию и копирует дефолтные файлы (конфиг + wallpaper).

Поддерживаются комментарии через `#`.

```ini
gaps 8
outer_gaps 8
mfact 0.55

$term alacritty
$launcher fuzzel

# Автостарт
exec-once swaybg -i ~/.config/flottywm/wallpaper.jpg -m fill
# exec-once waybar

$mod Alt

bind $mod+Return exec $term
bind $mod+d exec $launcher
bind $mod+Escape exit
bind $mod+q kill
bind $mod+Shift+space togglefloating
bind $mod+Shift+c reload

# Workspaces
bind $mod+1 workspace 1
bind $mod+2 workspace 2
# ...
bind $mod+Shift+1 movetoworkspace 1
bind $mod+Shift+2 movetoworkspace 2
# ...

# Focus
bind $mod+h focusprev
bind $mod+l focusnext
bind $mod+Left focusleft
bind $mod+Right focusright
bind $mod+Up focusup
bind $mod+Down focusdown

# Swap windows
bind $mod+Shift+h moveprev
bind $mod+Shift+l movenext

# Resize
bind $mod+Ctrl+Left resizeleft
bind $mod+Ctrl+Right resizeright
bind $mod+Ctrl+Up resizeup
bind $mod+Ctrl+Down resizedown
```

### Доступные команды

| Команда | Описание |
|---------|----------|
| `env <NAME> <VALUE>` | Переменная окружения |
| `exec <программа>` | Запустить программу |
| `exec-once <команда>` | Автостарт |
| `exit` | Завершить compositor |
| `workspace <N>` | Переключиться на воркспейс |
| `movetoworkspace <N>` | Переместить окно на воркспейс |
| `focusprev` / `focusnext` | Фокус по списку |
| `focusleft/right/up/down` | Фокус в направлении |
| `moveprev` / `movenext` | Поменять окно местами |
| `togglefloating` | Floating / tiled |
| `reload` | Перезагрузить конфиг |
| `kill` | Закрыть окно |
| `resizeleft/right` | Изменить ширину master-области |
| `resizeup/down` | Изменить размер floating окна |

### Настройки

| Параметр | Описание | По умолчанию |
|----------|----------|--------------|
| `gaps` | Отступы между окнами | `8` |
| `outer_gaps` | Внешние отступы | `8` |
| `mfact` | Доля экрана для master | `0.55` |

## Управление мышью

- **Win + Правая кнопка** — перетаскивание floating окна
- **Клик по окну** — фокус

## Лицензия

MIT
