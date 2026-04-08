# mouse-sync Roadmap

## Цель roadmap

Этот документ фиксирует ближайшие этапы развития проекта и помогает не смешивать в одном шаге:

- стабилизацию текущего Windows backend;
- проектирование profile/schema;
- добавление Linux backend'ов;
- будущий cross-platform mapping.

Roadmap не гарантирует точные сроки. Это список приоритетов и последовательности работ.

## Текущее состояние

Сделано:

- Windows capture;
- Windows apply;
- JSON serialization/deserialization;
- CLI команды `capture`, `apply`, `print`, `schema`, `version`.

Не сделано:

- строгая validation layer;
- automated tests;
- `x11-cinnamon` backend;
- backend abstraction layer;
- cross-backend mapping.

## Stage 0 - Stabilize current base

Статус: ближайший приоритет.

### Задачи

- добавить strict validation перед `apply`;
- проверить semantic correctness Windows registry fields;
- отделить parse/validate/apply этапы;
- добавить минимальные tests на JSON round-trip и invalid input;
- зафиксировать архитектурные решения в `docs/ARCHITECTURE.md`.

### Результат этапа

Проект должен безопасно отклонять неподходящие или неполные профили до изменения системных настроек.

## Stage 1 - Backend abstraction

Статус: после стабилизации базовой модели.

### Задачи

- спроектировать backend contract;
- вынести backend selection из `cli/main.cpp`;
- подготовить расширяемую схему регистрации backend'ов;
- определить идентификаторы backend'ов и требования к compatibility checks.

### Результат этапа

Добавление второго backend'а не должно требовать разрастания CLI в platform routing layer.

## Stage 2 - Linux KDE Wayland backend

Статус: базовый backend реализован, расширение продолжается.

### Целевая среда

- CachyOS;
- KDE Plasma;
- Wayland.

### Что нужно исследовать

- где KDE/Plasma хранит relevant pointer settings;
- какие значения реально влияют на perceived speed и acceleration;
- можно ли безопасно применять изменения через stable user-space interfaces;
- какие значения можно reliably capture/apply без compositor-specific hacks.

### Предполагаемые направления

- `kwriteconfig6`;
- KDE config files;
- libinput-related settings;
- возможные DBus / Plasma integration points.

### Что уже сделано

- backend `kde-wayland` добавлен в CLI;
- capture/apply реализованы через KWin DBus;
- Linux payload умеет хранить pointer settings per-device.

### Что ещё осталось по этому этапу

- расширить покрытие Plasma-specific settings;
- добавить tests на Linux profile parsing и invalid device matching;
- документировать ограничения device matching и supported properties.

### Результат этапа

У проекта уже есть первый рабочий Linux backend для одной конкретной среды, но его ещё нужно довести до более устойчивого и проверяемого состояния.

## Stage 3 - Linux X11 Cinnamon backend

Статус: второй Linux backend.

### Целевая среда

- Linux Mint;
- Cinnamon;
- X11;
- Muffin.

### Что нужно исследовать

- где Cinnamon/X11 хранит mouse settings;
- какова роль `xinput`, X resources и desktop settings;
- есть ли различие между session runtime values и persisted config;
- как привязать настройки к текущему pointer device или global desktop policy.

### Предполагаемые направления

- `xinput`;
- XInput properties;
- desktop settings storage;
- возможно `gsettings`, если часть поведения проходит через desktop stack.

### Результат этапа

Появляется backend `x11-cinnamon`, отдельно описанный и не смешанный с Wayland logic.

## Stage 4 - Schema refinement

Статус: после появления минимум двух backend'ов.

### Задачи

- пересмотреть top-level profile model;
- добавить более явный backend identifier в profile payload;
- определить schema migration policy;
- ввести compatibility rules между schema versions.

### Результат этапа

JSON format становится более строгим и устойчивым к росту числа backend'ов.

## Stage 5 - Cross-backend mapping

Статус: после появления нескольких рабочих backend'ов.

### Цель

Не просто сохранять и восстанавливать профиль в той же среде, а пытаться приблизить ощущение pointer movement между разными ОС и desktop stacks.

### Возможные задачи

- определить минимальный общий набор semantic settings;
- ввести нормализованное представление pointer behavior;
- добавить rules для approximate mapping между Windows и Linux backend'ами;
- документировать ограничения и неточности такого маппинга.

### Важное ограничение

Идеального one-to-one mapping между Windows, Wayland и X11, скорее всего, не будет. Цель этапа - предсказуемое приближение, а не математически точная эквивалентность.

## Stage 6 - Calibration and measurement

Статус: опционально после базовой поддержки backend'ов.

### Идеи

- измерение фактической скорости указателя;
- калибровка под привычное ощущение пользователя;
- возможно, профили для разных мышей или DPI presets.

### Результат этапа

Проект двигается от raw settings sync к user-perceived sync.

## Non-goals for now

Пока не являются приоритетом:

- GUI;
- поддержка всех Linux DE/WM;
- cloud sync;
- system service / daemon mode;
- автодетект и авто-применение при каждом старте ОС.

## Ближайшие practical next steps

Если двигаться по минимально рискованной траектории, следующий набор задач такой:

1. validation before apply;
2. Windows backend cleanup;
3. tests for profile parsing and invalid input;
4. tests for `kde-wayland` device matching and invalid input;
5. extend `kde-wayland` payload with additional relevant Plasma settings;
6. separate design note for `x11-cinnamon`.