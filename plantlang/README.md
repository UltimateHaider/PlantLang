# 🌿 PlantLang — Chloroplast v0.6

لغة برمجة مصممة لتُقرأ كالنثر الطبيعي.

## تشغيل سريع

```bash
node chloroplast.js run examples/01_basics.plnt
node chloroplast.js repl
node chloroplast.js --help
```

## مثال

```
MISSION: SAFE.
1\ CREATE name(TX) TO "حيدر".
1\ CREATE score(NUM) TO 87.
1\ SHOW "مرحباً " + name.
1\ IF score BETWEEN (80, 89), SHOW "جيد جداً ✓".
```

## المجلدات

- `core/`      — المفسر الأساسي
- `examples/`  — أمثلة كاملة
- `tests/`     — مجموعة الاختبارات (50 اختبار)

## الاختبارات

```bash
node chloroplast.js run tests/all.plnt
# → 🌿 جميع الاختبارات نجحت! (50/50)
```
