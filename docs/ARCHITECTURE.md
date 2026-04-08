# mouse-sync Architecture

## Назначение

`mouse-sync` - CLI tool для захвата и применения настроек мыши между разными ОС в multi-boot окружении. Текущая цель проекта не в том, чтобы побитово повторить все внутренние настройки каждой платформы, а в том, чтобы:

- сохранить максимально релевантный снимок пользовательских mouse settings;
- безопасно сериализовать его в JSON;
- применить профиль обратно в ту же или другую поддерживаемую среду;
- оставить место для будущего маппинга между разными backend'ами.

Сейчас проект находится на стадии рабочего skeleton-а: Windows backend уже реализован и проверен вручную, Linux backend'ы пока не реализованы.

## Текущая структура

Проект разделен на три основных слоя:

1. `core/`
   Содержит модель профиля и JSON serialization/deserialization.

2. `backends/`
   Содержит platform-specific код для capture/apply.

3. `cli/`
   Содержит пользовательский интерфейс командной строки и orchestration.

Текущая структура каталогов:

```text
core/
  include/mouse_sync/profile.hpp

backends/
  windows/
    src/windows_backend.hpp
    src/windows_backend.cpp

cli/
  main.cpp
```

## Текущее поведение

На текущем этапе поддерживаются команды:

- `capture`
- `apply`
- `print`
- `schema`
- `version`

Поддержанный backend сейчас один:

- `windows`

При `capture --os windows` CLI:

1. вызывает Windows backend;
2. формирует `MouseProfile`;
3. записывает JSON на диск.

При `apply --os windows` CLI:

1. читает JSON;
2. десериализует `MouseProfile`;
3. передает Windows snapshot в backend;
4. backend применяет SPI и registry settings.

## Слой данных

Сейчас в `core/include/mouse_sync/profile.hpp` хранится единый `MouseProfile`, внутри которого присутствуют сразу две секции:

- `windows`
- `linux`

Это допустимо для skeleton-а, но такая модель имеет ограничение: она позволяет существование частично заполненных профилей и не различает явно, какой backend является реальным источником данных.

### Текущее состояние модели

- есть top-level metadata: `mouse_sync_version`, `schema_version`, `created_at`, `source_os`;
- есть platform sections;
- JSON формат кросс-платформенный и человекочитаемый;
- строгой доменной валидации перед `apply` пока нет.

### Желаемое направление развития

По мере роста проекта модель должна эволюционировать в сторону более явного представления platform payload:

- профиль должен явно говорить, для какого backend'а он был создан;
- `apply` должен валидировать совместимость профиля и backend'а до первого side effect;
- частично заполненный профиль не должен считаться безопасно применимым по умолчанию.

Это особенно важно до добавления нескольких Linux backend'ов.

## Backend layer

Backend отвечает за две операции:

- `capture()`
- `apply()`

Текущий Windows backend использует:

- `SystemParametersInfoW` для speed и acceleration settings;
- `HKCU\Control Panel\Mouse` для registry snapshot;
- `WM_SETTINGCHANGE` для broadcast изменений.

### Принципы для backend'ов

Каждый backend должен:

- иметь четкую границу ответственности;
- инкапсулировать platform-specific API и storage details;
- делать best-effort capture, но не маскировать опасные apply errors;
- валидировать вход до применения изменений;
- быть тестируемым хотя бы на уровне serialization и basic validation.

### Почему Linux backend'ов будет несколько

Для Linux в рамках проекта ожидается минимум два отдельных backend'а:

1. `kde-wayland`
2. `x11-cinnamon`

Причина в том, что это не просто разные desktop environments, а разные модели доступа к mouse settings:

- Wayland ограничивает прямой универсальный доступ и обычно требует работу через compositor/desktop-specific interfaces;
- X11 чаще позволяет работать через `xinput` или X settings, но semantic mapping отличается;
- storage locations, naming и applied behavior в этих средах различаются.

Из этого следует важное архитектурное правило: в проекте нужно мыслить не абстрактным `linux` backend'ом, а конкретными backend adapters по среде выполнения.

## Предлагаемая целевая декомпозиция

Среднесрочно проект должен прийти к такой схеме:

1. `core`
   Доменная модель, schema versioning, validation, shared mapping utilities.

2. `backend interface`
   Единый контракт для platform adapters.

3. `backend implementations`
   `windows`, `kde-wayland`, `x11-cinnamon`, далее при необходимости другие.

4. `cli`
   Только parsing аргументов, выбор backend'а, вызов capture/apply/print.

Это позволит не раздувать `cli/main.cpp` по мере добавления новых платформ.

## Желаемый backend contract

Конкретный C++ interface пока не зафиксирован, но направление такое:

- backend имеет стабильный идентификатор;
- backend умеет сказать, доступен ли он в текущей среде;
- backend умеет захватить профиль своей среды;
- backend умеет применить профиль только своего формата;
- backend может вернуть диагностическую информацию, если среда не поддерживается.

Примерно на уровне концепции это выглядит так:

```text
Backend
  id()
  display_name()
  can_capture()
  can_apply(profile)
  capture()
  apply(profile)
```

Конкретную реализацию интерфейса стоит вводить тогда, когда появится второй реальный backend.

## Data flow

### Capture

```text
CLI
  -> backend selection
  -> backend.capture()
  -> profile validation / normalization
  -> JSON serialization
  -> file output
```

### Apply

```text
CLI
  -> file input
  -> JSON deserialization
  -> schema validation
  -> backend compatibility validation
  -> backend.apply()
```

Для `apply` принципиально важно, чтобы все проверки проходили до фактического изменения системных настроек.

## Основные архитектурные риски на текущем этапе

### 1. Слишком ранняя фиксация JSON модели

Текущая схема удобна для старта, но недостаточно строго различает platform-specific payload. Если оставить ее без изменений, добавление `kde-wayland` и `x11-cinnamon` создаст либо слишком общий и размытый JSON, либо большое количество опциональных полей.

### 2. CLI знает слишком много о backend'ах

Сейчас CLI напрямую зависит от конкретного backend include и platform compile definitions. Это допустимо с одним backend'ом, но плохо масштабируется.

### 3. Риск частичного apply

Если backend меняет часть системных настроек, а потом падает на следующем шаге, итоговое состояние может оказаться смешанным. Это означает, что validate phase должна быть отделена от side effects.

### 4. Отсутствие automated tests

Без тестов наиболее вероятны проблемы в:

- JSON schema evolution;
- validation;
- cross-version compatibility;
- безопасном отклонении неподходящих профилей.

## Принципы развития

При дальнейшем развитии проекта стоит придерживаться следующих правил:

1. Не пытаться рано построить универсальный cross-OS mapper для всех платформ сразу.
2. Сначала стабилизировать profile model и validation.
3. Добавлять backend'ы по одному и фиксировать их ограничения в документации.
4. Отличать capture fidelity от apply safety: неполный capture допустим, небезопасный apply - нет.
5. Любое platform-specific поведение документировать рядом с backend'ом и в `docs/`.

## Ближайший эволюционный путь

Рациональный следующий шаг после текущего skeleton-а:

1. добавить строгую валидацию профиля перед `apply`;
2. выделить backend selection из CLI в более явный слой;
3. спроектировать payload для `kde-wayland`;
4. затем отдельно спроектировать payload для `x11-cinnamon`;
5. только после этого думать о cross-backend mapping rules.

## Документирование изменений

Этот документ должен обновляться при каждом заметном архитектурном изменении:

- новый backend;
- изменение JSON schema;
- изменение backend contract;
- появление validation layer;
- появление mapping layer между платформами.