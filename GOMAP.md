# 🌿 PlantLang for Visual Studio Code

Syntax highlighting, snippets, and a custom dark theme for **PlantLang** (`.plnt` files).

> PlantLang is a programming language designed to read like natural prose.
> Write code the way you write a sentence — not the way you debug a cipher.

---

## Features

### Syntax Highlighting

Full semantic coloring for every PlantLang construct:

| Element | Color |
|---------|-------|
| Depth prefix (`1\`, `2\`) | 🟢 Green (brand color) |
| Keywords (`CREATE`, `IF`, `CYCLE`, `MATCH`) | 🟢 Green |
| Type names (`NUM`, `TX`, `LIST`, `MAP`) | 🟠 Orange |
| Storm types (`ZERO_STORM`, `ANY_STORM`) | 🔴 Red bold |
| Strings | 🔵 Blue |
| Numbers | 🔵 Light blue |
| Comments (`#`) | ⬛ Gray italic |
| Action names | 🟣 Purple |
| Class names (`SPECIES`, `BLOOM`) | 🟠 Orange |
| `SELF:prop` | 🔴 Red |
| `VERIFY` / `SUITE` | 🟡 Yellow |
| Library calls (`math:SQRT`) | 🔵 Cyan + Purple |
| HTTP keywords (`HARVEST`, `LISTEN BRANCH`) | 🟢 Green bold |

### Snippets

Type a prefix and press `Tab` to expand:

| Prefix | Expands to |
|--------|-----------|
| `mission` | `MISSION: SAFE.` |
| `create` | `CREATE name(NUM) TO value.` |
| `list` | `CREATE items(LIST) TO item1, item2.` |
| `map` | `CREATE data(MAP). LINK "key" WITH value IN data.` |
| `if` | `IF condition, body. 1\.` |
| `ifelse` | Full `IF / ORIF / ELSE` chain |
| `cycle` | `CYCLE item IN list, body. 1\.` |
| `cyclerange` | `CYCLE i FROM 1 TO 10, body. 1\.` |
| `season` | `SEASON condition, body. 1\.` |
| `match` | `MATCH value, IS ... YIELD ... ELSE YIELD. 1\\.` |
| `action` | Full ACTION definition |
| `reap` | `REAP result FROM action, arg.` |
| `weather` | `WEATHER / SHELTER / CALM` block |
| `species` | Full SPECIES + BLOOM template |
| `root` | `ROOT NAME TO value.` |
| `rootscope` | `ROOT_SCOPE CONFIG, LINK ... ROOT_SCOPE/.` |
| `plant` | `PLANT math.` |
| `braid` | `BRAID list1 WITH list2 AS result.` |
| `braidmap` | `BRAID keys WITH values AS result MAP.` |
| `harvest` | HTTP GET with response handling |
| `harvestpost` | HTTP POST with body and headers |
| `listen` | Full HTTP server with routing |
| `verify` | `VERIFY "label", condition.` |
| `suite` | `SUITE "Name", ... SUITE/.` |
| `verifyfrom` | `VERIFY "label", FROM action, args GIVES expected.` |
| `verifystorm` | `VERIFY "label", STORMS STORM_TYPE FROM expr.` |
| `pulse` | Reactive variable + WHENEVER watcher |
| `analyze` | `ANALYZE variable.` |
| `now` | `REAP ts FROM NOW FORMAT:STAMP.` |
| `plnt` | Full program template |

### PlantLang Dark Theme

A custom dark theme matching PlantLang's brand:
- Background: `#0d1117` (deep dark)
- Accent: `#3ee07f` (neon green — the PlantLang leaf color)
- Optimized for `.plnt` files but works great for all languages

**To activate:** `Ctrl+Shift+P` → `Color Theme` → `PlantLang Dark`

---

## Installation

### From VSIX (manual)

1. Download `plantlang-0.7.0.vsix`
2. Open VSCode → Extensions (`Ctrl+Shift+X`)
3. Click `···` → `Install from VSIX...`
4. Select the file

### From Command Line

```bash
code --install-extension plantlang-0.7.0.vsix
```

---

## Example

```plnt
MISSION: SAFE.
PLANT math.

ROOT MAX_SCORE TO 100.

1\ ACTION grade(score(NUM)),
2\   MATCH score,
3\     IS BETWEEN 90 100 YIELD GIVE "A".
3\     IS BETWEEN 80 89  YIELD GIVE "B".
3\     IS BETWEEN 70 79  YIELD GIVE "C".
3\     ELSE              YIELD GIVE "F".
2\   \\.
1\ /ACTION.

1\ REAP result FROM grade, 94.
1\ SHOW "Grade: " + result.
```

---

## Language Reference

Full documentation: [PlantLang README](https://github.com/plantlang/plantlang)

---

## Changelog

### v0.17.0
- Initial release
- Full syntax highlighting for all 93 keywords
- 25 code snippets
- PlantLang Dark color theme
