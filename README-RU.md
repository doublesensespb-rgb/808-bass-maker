# 808 BASS MAKER — справочник владельца

Здесь всё, что нужно, чтобы собирать, менять и выпускать плагин самому или с любым помощником: ИИ или программистом.
Версия на момент написания: **0.13**.

---

## 1. Что где лежит

| Что | Где |
|---|---|
| Исходный код (главное) | https://github.com/doublesensespb-rgb/808-bass-maker (ветка `main`) |
| Скачать весь код одним ZIP | https://github.com/doublesensespb-rgb/808-bass-maker/archive/refs/heads/main.zip |
| Готовые сборки последней версии + картинки интерфейса | ветка `builds`: https://github.com/doublesensespb-rgb/808-bass-maker/tree/builds |
| Автосборка (облако GitHub) | https://github.com/doublesensespb-rgb/808-bass-maker/actions |
| Инструкция для ИИ и программиста | `AGENTS.md`: https://github.com/doublesensespb-rgb/808-bass-maker/blob/main/AGENTS.md |
| Этот справочник | `README-RU.md`: https://github.com/doublesensespb-rgb/808-bass-maker/blob/main/README-RU.md |
| Сборка на своём компьютере | `tools/build-windows.bat` |
| Звук (движок) | `Source/BassEngine.h` |
| Параметры, LFO, пресеты | `Source/PluginProcessor.cpp` |
| Интерфейс | `Source/PluginEditor.cpp` |
| Заводские пресеты | `Source/BassEngine.h`, функция `factoryPresets()` |
| Твои пресеты | `Документы\808 BASS MAKER\Presets\*.808preset` |
| Родственный плагин (откуда LFO и дизайн) | GRAIN FORM: https://github.com/doublesensespb-rgb/granulum |

Код полностью твой. Ни я, ни Anthropic не нужны, чтобы его собирать и менять.

**Резервная копия.** Держи офлайн-архив `808-BASS-MAKER-0.12-offline.zip` на компьютере и ещё в одном месте:
флешка, облако. В нём код со всей историей, JUCE 8.0.10 (уже на своём месте), готовые сборки 0.12 для Windows и Mac
и этот справочник.

**Защита аккаунта.** Включи двухфакторную авторизацию GitHub: https://github.com/settings/security

---

## 2. Как собрать новую версию

### Вариант А: в облаке GitHub, одной кнопкой (ничего ставить не надо)
1. Открой https://github.com/doublesensespb-rgb/808-bass-maker/actions/workflows/build.yml
2. Справа нажми **Run workflow**, выбери ветку `main`, снова **Run workflow**.
3. Примерно через 15 минут `808-BASS-MAKER-Windows.zip`, `808-BASS-MAKER-macOS.zip` и картинки интерфейса
   появятся здесь: https://github.com/doublesensespb-rgb/808-bass-maker/tree/builds
4. Переименуй файлы под версию, например `808-BASS-MAKER-0.13-Windows.zip`.

Репозиторий приватный. GitHub даёт бесплатные минуты сборки каждый месяц, при этом Windows и Mac считаются дороже Linux.
Одна сборка — примерно час «облачных» минут, на бесплатном аккаунте этого хватает примерно на 30 сборок в месяц.
Тарифы: https://docs.github.com/en/billing/managing-billing-for-your-products/managing-billing-for-github-actions/about-billing-for-github-actions

### Вариант Б: на своём Windows-компьютере (работает без интернета)
Установить один раз, пока есть интернет:
- **Visual Studio 2022 Community**: https://visualstudio.microsoft.com/vs/community/
  При установке отметь **«Desktop development with C++»** («Разработка классических приложений на C++»).
- **CMake**: https://cmake.org/download/ — при установке отметь **«Add CMake to the system PATH»**.
- **Git**: https://git-scm.com/download/win — нужен только чтобы забирать код с GitHub и отправлять его туда.

Потом каждый раз:
1. Распакуй офлайн-архив (или папку с кодом). Папка `JUCE` должна лежать рядом с `CMakeLists.txt`,
   в офлайн-архиве она уже на месте.
2. Дважды кликни `tools\build-windows.bat`.
3. Готовый плагин появится в папке `out`: `808 BASS MAKER.vst3` и `808 BASS MAKER.exe` (отдельная программа).

Первая сборка идёт 10–20 минут, следующие быстрее.
Mac-версию можно собрать только на Mac (Xcode) или через вариант А.

### Установка в FL Studio
Скопируй `808 BASS MAKER.vst3` в `C:\Program Files\Common Files\VST3` (старый замени), затем в FL:
Options → Manage plugins → Find more plugins (или Rescan).
Mac: инструкция в `README.txt` внутри мак-архива.

---

## 3. Как менять плагин без меня

### С другим ИИ-агентом
Подойдёт любой агент, который умеет работать с кодом и GitHub: Claude Code (https://claude.com/claude-code),
OpenAI Codex (https://openai.com/codex), Cursor (https://cursor.com) и т. п. Готовое первое сообщение агенту:

> Это мой VST-плагин 808 BASS MAKER: https://github.com/doublesensespb-rgb/808-bass-maker
> Сначала полностью прочитай AGENTS.md и следуй правилам оттуда.
> Я не программист: делай всё сам. Сначала согласуй со мной правки текстом, покажи картинку дизайна до сборки
> и не запускай сборку без моей команды «собирай». Отвечай по-русски и всегда давай ссылки.
> Никогда не меняй коды плагина (PLUGIN_MANUFACTURER_CODE, PLUGIN_CODE, BUNDLE_ID, PRODUCT_NAME) и ID параметров.
> Задача: <что хочешь изменить>

### С программистом
Это обычный проект на **C++17 + JUCE 8 + CMake**. Ищи «JUCE developer»:
- форум JUCE: https://forum.juce.com (раздел Jobs)
- KVR: https://www.kvraudio.com/forum/
- Upwork: https://www.upwork.com (поиск «JUCE»)
- Kwork: https://kwork.ru, FL.ru: https://www.fl.ru

Дай ему ссылку на репозиторий и попроси сначала прочитать `AGENTS.md`.

### Что нельзя менять (иначе сломаются проекты и пресеты пользователей)
1. Коды плагина в `CMakeLists.txt`: `PLUGIN_MANUFACTURER_CODE Thmt`, `PLUGIN_CODE B808`,
   `BUNDLE_ID com.thematic.808bassmaker`, `PRODUCT_NAME "808 BASS MAKER"`.
2. Названия параметров (`ParamID` в `Source/PluginProcessor.h`). Убрать ручку можно, параметр — нет
   (так сделано с GLIDE в 0.11).
3. Списки шумов и типов драйва: новое добавлять только в конец.
4. Формат пресетов `.808preset`: только добавлять новое, не переименовывать старое.

Обязательно скажи это любому помощнику. В `AGENTS.md` это уже написано.

### Как называть версии
0.12 → 0.13 → 0.14 → … → 1.0 (первый релиз). Номер ставится в `CMakeLists.txt`, строка `project(BassMaker VERSION 0.1.3)`
(0.14 = `0.1.4`, 1.0 = `1.0.0`), и в надписи «v0.13» в интерфейсе (`Source/PluginEditor.cpp`).

---

## 4. Чек-лист к релизу

**Юридическое**
- [ ] Лицензия JUCE: бесплатно (Starter), пока доход с плагинов до $20 000 в год; выше — платный тариф.
      Проверь актуальные условия перед продажей: https://juce.com/get-juce/
- [ ] VST3 с октября 2025 под лицензией MIT, подписывать ничего не нужно. Логотип «VST» на сайте — по правилам Steinberg:
      https://www.steinberg.net/developers/
- [ ] Проверить, что имя 808 BASS MAKER не занято: поиск на KVR (https://www.kvraudio.com), Plugin Boutique,
      реестр товарных знаков https://branddb.wipo.int
- [ ] EULA (лицензионное соглашение покупателя).

**Mac**
- [ ] Apple Developer Program ($99 в год): https://developer.apple.com/programs/ — нужен для подписи и нотаризации,
      иначе macOS ругается при установке. После регистрации подпись встраивается в автосборку.

**Windows**
- [ ] Установщик вместо zip: Inno Setup, https://jrsoftware.org/isinfo.php
- [ ] (по желанию) подпись кода, чтобы не было предупреждений SmartScreen.

**Качество**
- [ ] Проверка pluginval: https://github.com/Tracktion/pluginval (Windows и Mac), auval для Logic.
- [ ] Проверить в FL Studio, Ableton, Reaper, Bitwig, Logic.
- [ ] 20–50 заводских пресетов.

**Продажа**
- [ ] Площадка: свой сайт + Gumroad (https://gumroad.com) или Lemon Squeezy (https://www.lemonsqueezy.com);
      магазины Plugin Boutique (https://www.pluginboutique.com), KVR Marketplace (https://www.kvraudio.com/marketplace).
- [ ] Защита от копирования: без защиты, ключи активации или ключи площадки — решить.
- [ ] Страница плагина, видео-демо, аудиопримеры, почта для поддержки.

---

## 5. Если что-то пошло не так

- **FL показывает старую версию.** Удали старый `808 BASS MAKER.vst3` из `C:\Program Files\Common Files\VST3`,
  положи новый и пересканируй плагины в FL.
- **Сборка на GitHub упала.** Actions → открой красный запуск → прочитай ошибку → передай текст ИИ-агенту или программисту.
- **`build-windows.bat` пишет «CMake not found».** Переустанови CMake с галочкой «Add CMake to the system PATH»
  и перезагрузи компьютер.
- **`build-windows.bat` падает на компиляции.** Проверь, что в Visual Studio Installer отмечено
  «Desktop development with C++». Текст ошибки (прокрути окно вверх) передай помощнику.
- **Нет папки JUCE и нет интернета.** Возьми папку `JUCE` из офлайн-архива и положи рядом с `CMakeLists.txt`.
- **Пропал доступ к GitHub.** Всё есть в офлайн-архиве. Создай новый репозиторий и загрузи туда папку с кодом
  (в ней вся история: папка `.git`), поправь ссылки в этом справочнике и в `AGENTS.md`.
- **Проект в FL открылся без звука или со сброшенными ручками.** Значит, кто-то поменял коды плагина или ID параметров.
  Верни их как в разделе 3.
