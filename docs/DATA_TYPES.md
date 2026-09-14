# PlantLang v0.50.7 — Data Types Report

**Scope:** Complete analysis of every type, conversion function, default, limit, and C representation in the PlantLang compiler and runtime.

---

## 1. Primitive Types

PlantLang uses a **single opaque pointer** (`tx_t = void*`) as the universal runtime representation. All type information is either:
- **Compile-time only** (type annotations used for codegen C type mapping)
- **Runtime-inferred** (magic numbers, pointer ranges, string content inspection)

### 1.1 Type Keyword → C Type Mapping

| PlantLang Type | C Equivalent | Size | Range | Default | Notes |
|---|---|---|---|---|---|
| `NUM` | `long` | 8 bytes (64-bit) | -(2^63) to 2^63-1 | `0` | Integer numbers |
| `SCL` | `long` (or `double` for decimals) | 8 bytes | -(2^63) to 2^63-1 | `0` | Scalar/decimal; decimals wrap via `_from_double` |
| `FACT` | `int` | 4 bytes | -(2^31) to 2^31-1 | `0` | Boolean-like fact values |
| `TX` | `tx_t` (void*) | 8 bytes (pointer) | N/A | `""` (empty string) | Text/string; default type when unspecified |
| `LIST[T]` | `PlantArray*` | 16 bytes (pointer) | N/A | `plant_list_make(0)` | Dynamic array of T values |
| `MAP` | `PlantArray*` (pair-list) | 16 bytes (pointer) | N/A | `plant_map_create()` | Key-value pair-list (kind=1) |
| `ENUM X` | `tx_t` (int or name string) | 8 bytes (pointer) | 0 to 65535 (int) | first member | Enum member as integer or name string |
| `MATH` | `PlantMath*` | 16 bytes (pointer) | N/A | `plant_math_create("0")` | Symbolic math expression (v0.50.0h) |

**Source:** `src/plantc/codegen_c.plant:5344-5362` (`plant_ctype`)

### 1.2 Reference Types

| PlantLang Type | C Equivalent | Notes |
|---|---|---|
| `REF NUM` | `long*` | Pointer to numeric |
| `REF FACT` | `int*` | Pointer to fact |
| `REF LIST[T]` | `PlantArray**` | Pointer to list |
| `REF TX` | `tx_t*` | Pointer to text |
| `REF STRUCT X` | `plant_X*` | Pointer to struct |

**Source:** `src/plantc/codegen_c.plant:5356-5361`

---

## 2. Composite Types

### 2.1 LIST (Dynamic Array)

**C Struct:**
```c
#define PLANT_ARRAY_MAGIC 0x504C4152 /* "PLAR" */

typedef struct PlantArray {
    uint64_t magic;     /* PLANT_ARRAY_MAGIC for reliable type detection */
    int64_t  count;
    int64_t  capacity;
    char**   items;
    int8_t   kind;      /* 0 = LIST, 1 = MAP (pair-list) */
} PlantArray;
```

**Declaration:**
```plantlang
CREATE items (LIST[NUM]) TO [1, 2, 3].
CREATE names (LIST[TX]) TO ["a", "b", "c"].
LET mixed (LIST) TO [1, "two", TRUE].
```

**Usage:**
```plantlang
REAP len FROM COUNT, items.          # length
REAP val FROM FIRST, items.          # first element
REAP val FROM LAST, items.           # last element
REAP sum FROM SUM, items.            # numeric sum
REAP rev FROM REVERSE, items.        # reversed copy
REAP flat FROM FLATTEN, items.       # flatten nested
REAP chunk FROM CHUNK, items, 2.     # chunk into sublists
REAP zip FROM ZIP, a, b.             # element-wise pairs
REAP uniq FROM UNIQUE, items.        # dedup copy
REAP avg FROM AVERAGE, items.        # numeric average
REAP med FROM MEDIAN, items.         # numeric median
REAP sorted FROM SORT, items.        # ascending sort
REAP idx FROM INDEX_OF, items, 5.    # first index ("-1" if absent)
REAP filt FROM FILTER_GT, items, 3.  # filter > threshold
REAP filt FROM FILTER_LT, items, 3.  # filter < threshold
items:push("x").                     # append via method
items:pop().                         # remove last
```

**Limits:** No explicit max size; bounded by available memory.

### 2.2 MAP (Key-Value Pair-List)

**C Struct:** Same `PlantArray` with `kind = 1`. Stored as alternating key-value pairs: `["key1", val1, "key2", val2, ...]`.

**Declaration:**
```plantlang
CREATE m (MAP) TO { "name": "plant", "year": 2026 }.
LET empty (MAP) TO {}.
```

**Usage:**
```plantlang
REAP val FROM _map_get, m, "name".           # get value
m:put("color", "green").                     # set value
REAP exists FROM _map_has, m, "name".        # check key
m:pop().                                     # remove last pair
REAP keys FROM _map_keys, m.                 # list of keys
REAP str FROM plant_map_to_string, m.         # string repr
```

### 2.3 STRUCT (Record Type)

**C Struct:** Generated per-type, e.g. `plant_Point`:
```c
typedef struct plant_Point {
    long x;
    long y;
} plant_Point;
```

**Declaration:**
```plantlang
STRUCT Point {
    x: NUM,
    y: NUM
}.
```

**Usage:**
```plantlang
LET p (STRUCT Point) TO Point{3, 4}.
REAP px FROM _map_get, p, "x".        # field access via map
```

### 2.4 ENUM (Enumeration)

**C Representation:** Stored as `tx_t` — either the integer index (0-based) or the member name string.

**Declaration:**
```plantlang
ENUM Color {
    RED, GREEN, BLUE
}.
```

**Usage:**
```plantlang
LET c TO Color:RED.              # c = 0 (integer)
REAP name FROM _from_enum, c, "RED,GREEN,BLUE".  # name = "RED"
REAP idx FROM _to_enum, "GREEN", "RED,GREEN,BLUE".  # idx = 1
SHOW Color:RED.                  # displays "RED"
```

**Conversion Functions:**
- `_from_enum(idx, names_csv)` — integer to name string
- `_to_enum(name, names_csv)` — name string to integer
- Values >= 65536 are treated as already-converted name strings (idempotent)

### 2.5 SPECIES (Class-like Type)

**Declaration:**
```plantlang
SPECIES Animal {
    name: TX,
    age: NUM
}.
```

**Usage:**
```plantlang
LET dog (SPECIES Animal) TO BLOOM Animal("Rex", 5).
dog:method_name().
```

**Fields:** Map-backed; defaults to `""` for unset fields. Supports inheritance via `FROM`, method dispatch via `BLOOM`, and interface conformance via `IMPLEMENTS`.

### 2.6 ACTION (Function Type)

**Declaration:**
```plantlang
ACTION add(a(NUM), b(NUM)) -> NUM,
    GIVE a + b.
/ACTION.
```

**Usage:**
```plantlang
REAP result FROM add, 3, 4.
```

**Features:** Generics (`<T>`), mission modes (`WITH MISSION SAFE`), priority (`WITH PRIO 5`), result types (`-> Result<T,E>`).

### 2.7 OPTION / RESULT (Tagged Union)

**C Struct:**
```c
typedef struct PlantTagged {
    int       tag;      /* 0=Some/Ok, 1=None, 2=Err */
    void*     payload;  /* heap-allocated value */
    int       kind;     /* 0=Option, 1=Result */
} PlantTagged;
```

**Usage (runtime helpers, not language-level syntax):**
```plantlang
REAP opt FROM plant_option_some, value.
REAP none FROM plant_option_none.
REAP is_some FROM plant_option_is_some, opt.
REAP val FROM plant_option_value, opt.

REAP ok FROM plant_result_ok, value.
REAP err FROM plant_result_err, error_msg.
REAP is_ok FROM plant_result_is_ok, result.
```

### 2.8 CALLBACK (Function Pointer)

**C Type:**
```c
typedef tx_t (*plant_cb_t)(long ctx, tx_t val);
```

**Usage:** FFI callbacks for C function pointer parameters.

---

## 3. Special Types

### 3.1 NULL

**C Representation:** `(tx_t)0` or `NULL`

**Usage:**
```plantlang
LET x TO NULL.
IF x IS NULL, SHOW "empty". /IF.
```

**Truthiness:** Falsy. `plant_typeof(NULL)` returns `"null"`.

### 3.2 VOID

**Purpose:** Represents absence of value. Not a true type — used in action return annotations.

**Status:** M (missing) as a standalone type keyword. Actions without `->` return type implicitly return void.

### 3.3 STORM (Exception Object)

**C Struct:** ARC-managed `{type, message}` object created via `storm()` factory.

**Usage:**
```plantlang
THROW storm("NETWORK_STORM", "connection failed").
WEATHER
    ...
SHELTER AS e
    REAP msg FROM _map_get, e, "message".
CALM.
```

**Fields:** `type` (storm kind string), `message` (description), `file` (source path), `line` (source line).

### 3.4 ANY

**Purpose:** Dynamic type inspection via `TYPEOF`/`ANALYZE`.

**Usage:**
```plantlang
REAP info FROM TYPEOF, x.        # returns "int"/"string"/"map"/"list"/"null"
REAP info FROM ANALYZE, x.       # returns {type, size, keys} MAP
```

### 3.5 JSON (PlantJson)

**C Struct:**
```c
typedef struct PlantJson {
    int   kind;   /* 0=null 1=bool 2=num 3=str 4=arr 5=obj */
    void* val;    /* char* for kinds 1/2/3; PlantArray* for 4/5 */
} PlantJson;
```

**Usage:**
```plantlang
REAP j FROM json_parse, json_string.
REAP v FROM json_get, j, "key".
REAP v FROM json_at, j, 0.
REAP n FROM json_len, j.
REAP k FROM json_kind, j.
```

---

## 4. Type Declaration Syntax

### 4.1 Variable Creation

```plantlang
# CREATE with type annotation
CREATE x (NUM) TO 5.
CREATE name (TX) TO "hello".
CREATE items (LIST[NUM]) TO [1, 2, 3].
CREATE m (MAP) TO { "key": "val" }.

# CREATE with type inference
CREATE x TO 5.             # inferred as NUM
CREATE name TO "hello".    # inferred as TX
```

### 4.2 Variable Binding

```plantlang
# LET with type annotation
LET y (NUM) TO 10.
LET msg (TX) TO "world".

# LET with type inference
LET z TO 20.               # inferred as NUM
```

### 4.3 Constants

```plantlang
# Block-scoped constant
CONST PI TO "3.14159".

# Global constant (auto-elevated to ROOT scope)
ROOT MAX_RETRIES TO "3".
ROOT_SCOPE APP_NAME TO "MyApp".
```

### 4.4 Type Aliases

```plantlang
TYPE UserID = NUM.
TYPE NameMap = MAP.
```

### 4.5 Struct Declarations

```plantlang
STRUCT Point {
    x: NUM,
    y: NUM
}.

STRUCT Person {
    name: TX,
    age: NUM
}.
```

### 4.6 Enum Declarations

```plantlang
ENUM Direction {
    NORTH, SOUTH, EAST, WEST
}.

ENUM Status {
    ACTIVE, INACTIVE, PENDING
}.
```

### 4.7 Species Declarations

```plantlang
SPECIES Animal {
    name: TX,
    age: NUM
}.

SPECIES Dog FROM Animal {
    breed: TX
}.
```

### 4.8 Interface Declarations

```plantlang
INTERFACE Drawable {
    BLOOM draw().
}.
```

---

## 5. Type Conversion Functions

### 5.1 Numeric Conversions

| Function | From | To | Source |
|---|---|---|---|
| `_from_long(n)` | `long` | `tx_t` (text) | `plant_compat.h:86` |
| `_from_double(d)` | `double` | `tx_t` (text) | `plant_compat.h:91` |
| `_from_digit(n)` | `long` (0-9) | `tx_t` (static string) | `plant_compat.h:86` |
| `_to_long(s)` | `tx_t` (text) | `long` | `plant_compat.h:75` |
| `_from_ffi_num(n)` | `long long` | `tx_t` (text) | `plant_compat.h:96` |
| `plant_rw_arg_long(v)` | `tx_t` | `long` (SAFE wire) | `plant_compat.h:80` |

### 5.2 Enum Conversions

| Function | From | To | Source |
|---|---|---|---|
| `_from_enum(idx, names)` | int index | name string | `plant_compat.h:106-137` |
| `_to_enum(name, names)` | name string | int index | `plant_compat.h:144-162` |

### 5.3 Cast Macros

| Macro | Cast | Source |
|---|---|---|
| `_S(x)` | `tx_t` → `const char*` | `plant_compat.h:59` |
| `_P(x)` | `tx_t` → `PlantArray*` | `plant_compat.h:60` |
| `_L(x)` | `tx_t` → `long` | `plant_compat.h:61` |
| `_POS(x)` | `tx_t` → `long` (via `_to_long`) | `plant_compat.h:62` |

### 5.4 Implicit Conversion Rules

1. **Unspecified type → TX:** If no type annotation is given, the variable defaults to `TX` (tx_t).
2. **Numeric literals → long:** Integer literals compile to raw `long` C values; no wrapping needed.
3. **Decimal literals → _from_double:** Decimal numbers wrap in `_from_double()` for list/map contexts.
4. **String literals → tx_t:** Quoted strings pass through as `const char*` cast to `tx_t`.
5. **TRUE/FALSE → "1"/"0":** Boolean literals compile to string representations.
6. **NULL → (tx_t)0:** Null compiles to a null pointer.
7. **Enum member → int or name:** Depending on context, enums are either integer indices or name strings.

### 5.5 Explicit Conversion Rules

- `CONVERT` keyword: M (not supported in compiler)
- `_from_long()` / `_to_long()`: Explicit numeric ↔ text conversion
- `_from_enum()` / `_to_enum()`: Explicit enum ↔ integer conversion
- `plant_typeof()`: Runtime type introspection returning a type name string
- `plant_analyze()`: Runtime introspection returning `{type, size, keys}` MAP

---

## 6. Type Checking

### 6.1 Static vs Dynamic

PlantLang uses a **hybrid approach**:
- **Compile-time:** Type annotations are used for C codegen type mapping (`plant_ctype`), but the compiler does NOT enforce type correctness at compile time.
- **Runtime:** Type identity is determined by magic numbers, pointer ranges, and string content.

### 6.2 Type Enforcement

- **No compile-time type errors:** The compiler does not reject type mismatches.
- **Runtime type detection:** Functions like `plant_typeof()` and `plant_analyze()` inspect types at runtime.
- **Truthiness rules:** Conditions accept any `tx_t` value; falsy values are `NULL`, `"0"`, `"false"`, `"FALSE"`, and empty strings.

### 6.3 Type Mismatch Behavior

| Scenario | Behavior |
|---|---|
| Numeric operation on string | `atol()` returns 0 for non-numeric strings |
| String operation on numeric | `strlen()`/`strstr()` operate on the text representation |
| List operation on non-list | Functions return empty list `[]` or `"0"` |
| Map access on non-map | `_map_get()` returns `NULL` |

---

## 7. Runtime Representation

### 7.1 Type Detection Strategy

The runtime identifies types via multiple heuristics:

| Type | Detection Method |
|---|---|
| LIST/MAP | `magic == PLANT_ARRAY_MAGIC (0x504C4152)` |
| ENUM | Values < 65536 are integers; values >= 65536 are name strings |
| NUM | `atol()` succeeds on string content |
| BOOL | String is `"TRUE"`, `"true"`, `"1"`, `"FALSE"`, `"false"`, `"0"` |
| NULL | Pointer is `(tx_t)0` |
| STRUCT | Magic prefix in struct name |
| JSON | `PlantJson` struct with `kind` field (0-5) |
| OPTION/RESULT | `PlantTagged` struct with `tag` field (0-2) |
| CLOSURE | Function pointer with context |

### 7.2 Type Introspection

```c
tx_t plant_typeof(tx_t v);    /* Returns: "int"/"string"/"map"/"list"/"closure"/"null" */
tx_t plant_analyze(tx_t v);   /* Returns: {type, size, keys} MAP */
```

---

## 8. Type Limits

| Type | Limit | Value | Source |
|---|---|---|---|
| String max length | `PLANT_MAX_STRING_LEN` | 4096 chars | `plant_runtime.c` |
| Enum idempotent threshold | values >= 65536 | treated as name strings | `plant_compat.h:110` |
| SAFE worker raw int threshold | values < 4096 | packed as raw pointers | `plant_compat.h:80` |
| WEATHER exit list max | `PLANT_WEATHER_EXIT_MAX` | 64 handles per frame | `plant_runtime.h:105` |
| HTTP response max | recv loop break | 1MB (1048576 bytes) | `plant_runtime.c:361` |
| Closed FDs tracker | `_plant_closed_fds` | 128 entries | `plant_runtime.c:125` |
| Pending data buffers | `_plant_pending` | 64 entries | `plant_runtime.c:169` |
| String slab block | `PLANT_SLAB_BLOCK` | 64 bytes | `plant_runtime.h:315` |
| FFI error codes | enum | 0-5 (OK, TYPE, DEPTH, CALLBACK, MEMORY, SIGNATURE) | `plant_compat.h:816-821` |
| List average/median | empty list | returns `"0"` | `plant_runtime.h:222-223` |
| math_log | x <= 0 | returns `"ERR: math_log(x): ..."` | `plant_compat.h:567` |
| math_sqrt | negative | returns `"0"` | `plant_compat.h:559` |

---

## 9. Summary

### Type Counts

| Category | Count | Types |
|---|---|---|
| **Primitive** | 6 | NUM, SCL, FACT, TX, BOOL (via literals), NULL |
| **Composite** | 8 | LIST, MAP, STRUCT, ENUM, SPECIES, ACTION, CALLBACK, MATH |
| **Special** | 5 | VOID, STORM, ANY, JSON, OPTION/RESULT |
| **Reference** | 4 | REF NUM, REF FACT, REF LIST, REF TX |
| **Total** | **23** | |

### Primitive vs Composite

| Category | Types | C Representation |
|---|---|---|
| **Primitive** | NUM, SCL, FACT, TX | `long`, `int`, `tx_t` (all stored as `tx_t` at runtime) |
| **Composite** | LIST, MAP, STRUCT, ENUM, SPECIES, MATH | `PlantArray*`, generated structs, map-backed objects, `PlantMath*` |
| **Functional** | ACTION, CALLBACK | Function pointers with context |
| **Special** | NULL, VOID, STORM, ANY, JSON, OPTION, RESULT | Null pointer, void, ARC objects, tagged unions |

### Missing Compared to C

| C Type | PlantLang Status | Notes |
|---|---|---|
| `float` / `double` | M | Decimals stored as text strings (`_from_double`) |
| `char` | M | Single characters stored as 1-char strings |
| `unsigned int` | M | All integers are signed `long` |
| `short` | M | No 16-bit integer type |
| `long long` | M | 64-bit only via `_from_ffi_num` choke point |
| `struct` (anonymous) | P | Only named structs supported |
| `union` | M | No union type |
| `function pointer` | P | Only via `CALLBACK` FFI |
| `array` (fixed-size) | M | Only dynamic `PlantArray` |
| `pointer arithmetic` | M | No direct pointer manipulation |
| `bitwise types` | M | Operations exist but no dedicated types |
| `complex` | P | Supported via `PlantComplex` struct and `MATH_COMPLEX_*` built-ins (v0.50.2) |
| `decimal` | P | Stored as text, not native `double` |

### Recommendations

1. **Add native `double` type:** Currently decimals are stored as text strings, requiring repeated `atol`/`strtod` conversions. A native `DOUBLE` type with `plant_double_create`/`plant_double_value` would improve numeric performance.

2. **Add `char` type:** Single characters are currently 1-char strings. A dedicated `CHAR` type with `plant_char_create`/`plant_char_value` would be more efficient for character manipulation.

3. **Add `unsigned` types:** Currently all integers are signed. `UNUM`/`UFACT` types would enable unsigned arithmetic and bit manipulation.

4. **Add fixed-size arrays:** `ARRAY[T, N]` would enable stack-allocated fixed-size arrays for performance-critical code.

5. **Add anonymous structs:** Currently structs must be named. Anonymous struct literals would improve inline data definitions.

6. **Add `union` type:** A tagged union type would enable efficient variant data without map overhead.

7. **Add `decimal` type with native precision:** For financial/mathematical applications requiring exact decimal representation.

8. **Add compile-time type checking:** The current runtime-only type detection allows silent type mismatches. A gradual type system would catch errors earlier.

---

*Report generated for PlantLang v0.50.7. Sources: `runtime/c/plant_runtime.h`, `runtime/c/plant_compat.h`, `runtime/c/plant_runtime.c`, `src/plantc/codegen_c.plant`, `src/plantc/parser.plant`, `src/plantc/lexer.plant`.*
