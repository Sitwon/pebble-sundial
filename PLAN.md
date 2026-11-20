# Sundial — TDD implementation plan

**Audience:** a smaller coding model working one step at a time.
**Source of truth for requirements:** `SPEC.md`. If this plan and the spec disagree, the spec wins; note the difference in the commit message.
**Goal:** ship a working emery + gabbro watchface, tested at every layer.

---

## 0. How to execute this plan

1. Do **one step per commit**. Do not start the next step until the current step's tests pass.
2. Work red → green → refactor:
   - write the failing test first (it must fail for the right reason),
   - write the smallest code that passes,
   - clean up only the code you just wrote.
3. Run `./test/run.sh` before every commit. Run `pebble build` before every commit in phases 0, 5, 6, 7.
4. Never change an expected value just to make a test pass. If a reference is wrong, fix the test **and** say why in the commit.
5. No `float` or `double` in `src/c/`. Fixed point only. `./test/run.sh` enforces this.
6. No new dependencies except `@rebble/clay` (phase 6). No features beyond the current step.
7. If a step fails three times in a row: revert to the last commit, write down the actual error, and stop. Do not thrash.
8. Keep `SPEC.md` unchanged. Record deviations in `DECISIONS.md` (created in step 0.1).

### Test layers

| Layer | Method | Command |
|---|---|---|
| `fixmath`, `trig` | host asserts | `./test/run.sh` |
| solar table | Python asserts + freshness check | `./test/run.sh` |
| `solar`, `dial`, `theme`, `location` | host asserts | `./test/run.sh` |
| pkjs settings parsing | Node's test runner | `node --test test/` |
| rendering | build + emulator screenshot + `test/check_shot.py` | `pebble build`, `pebble screenshot`, `python3 test/check_shot.py ...` |
| integration | emulator time/location injection, relaunch, soak | phase 7 |

Host tests compile the math modules against `test/shim/trig_host.c`, so no Pebble SDK is needed for phases 1–4.

---

## 1. Target file tree

```
package.json
wscript
.gitignore
README.md
SPEC.md
PLAN.md
DECISIONS.md
src/c/main.c            lifecycle, tick, AppMessage, persistence, window
src/c/sundial.h         shared location/state structs only
src/c/fixmath.h/.c      clamp, q16 multiply, deg↔trig, isqrt
src/c/trig.h            tri_sin / tri_cos abstraction
src/c/trig.c            Pebble implementation (sin_lookup/cos_lookup)
src/c/solar.h/.c        table interpolation, AST, hour angle, sun vector
src/c/solar_table.h     generated, do not hand-edit
src/c/dial.h/.c         latitude clamp, gnomon, hour ring, projection, hull
src/c/theme.h/.c        day/golden/night + shadow strength
src/c/render.h/.c       layout, brass, engraving, numerals, shadow, blade
src/c/location.h/.c     coordinate validation (pure)
src/pkjs/index.js       GPS, AppMessage, Clay
src/pkjs/settings.js    pure parsing/validation, imported by Node tests
src/pkjs/config.js      Clay config definition
test/run.sh             host test entry point
test/test.h             CHECK/CHECK_NEAR macros
test/shim/trig_host.c   libm implementation of trig.h for host tests
test/test_fixmath.c
test/test_trig.c
test/test_solar.c
test/test_dial.c
test/test_theme.c
test/test_location.c
test/test_solar_table.py
test/settings.test.mjs
test/check_shot.py      screenshot luminance probes
tools/gen_solar_table.py
docs/shots/             committed screenshots
```

---

## 2. Decisions taken in this plan

These are within the spec's stated latitude; write them into `DECISIONS.md` in step 0.1.

1. **Hour-ring test oracle is analytic, not a second ephemeris.** The Python table test checks day-level regression values; `test_solar.c` checks the four hand-verified cases in `SPEC.md` §7.1. This is enough to catch the realistic failure modes.
2. **Numerals are vector Roman glyphs** (I/V/X stroke construction), per the spec. `FONT_KEY_GOTHIC_14` on emery, `FONT_KEY_GOTHIC_18` on gabbro is the fallback, used only if the glyphs look crude in emulator testing.
3. **Theme thresholds (per spec §3.5, unchanged):** day when `uz ≥ sin 6°`, golden when `0 ≤ uz < sin 6°` (shadow fades out below `sin 2°`), night when `uz ≤ −1°`. Shadow bands: 3 at `uz ≥ sin 6°`, 2 at `≥ sin 4°`, 1 at `≥ sin 2°`, 0 below. There is no band between `−1°` and `0`; night starts exactly at `uz ≤ sin(−1°) = −1144`.
4. **Shadow filling** is a scanline fill clipped row-by-row to the dial circle. No polygon/circle clipping code.
5. **No pixel-perfect golden images.** Screenshots are checked with luminance probes (`check_shot.py`) plus review by the agent.
6. **Clay** uses `@rebble/clay` 1.1.0.
7. **Hour-ring sign.** SPEC §3.4 gives the northern-hemisphere magnitude `tanθ = sin|φ|·tanH`. The implementation uses the signed-latitude vector `(sin|φ|·sinH, pole·cosH)`, which yields the exact southern mirror required by §3.3 and §3.6. Do not add a second mirror pass.

---

# Phase 0 — walking skeleton

### Step 0.1 — project scaffold

**Create**

- `package.json` (UUID from `python3 ~/.config/opencode/skills/pebble-watchface/scripts/generate_uuid.py`):

```json
{
  "name": "sundial",
  "author": "Adam",
  "version": "1.0.0",
  "keywords": ["pebble-app"],
  "private": true,
  "dependencies": {},
  "pebble": {
    "displayName": "Sundial",
    "uuid": "<GENERATED-UUID>",
    "sdkVersion": "3",
    "enableMultiJS": true,
    "targetPlatforms": ["emery", "gabbro"],
    "watchapp": { "watchface": true },
    "capabilities": ["location"],
    "messageKeys": [
      "REQUEST_LOCATION", "LAT_E4", "LON_E4",
      "LOCATION_SOURCE", "LOCATION_VALID"
    ],
    "resources": { "media": [] }
  }
}
```

- `wscript`: copy `~/.config/opencode/skills/pebble-watchface/templates/wscript.template` unchanged.
- `.gitignore`: `build/`, `node_modules/`, `*.pyc`, `__pycache__/`.
- `DECISIONS.md`: paste §2 above.
- `README.md`: build/test/emulator commands (can be filled in as the plan proceeds).
- `src/c/main.c`: window + one `CanvasLayer` that fills the screen black and draws a hard-coded brass circle centred on the layer bounds (platform-agnostic: centre = bounds mid, radius = `min(w,h)/2 − 6`, so both emery and gabbro render correctly).
- `src/pkjs/index.js`: empty file (Pebble needs it when `enableMultiJS` is true).

**Verify**

```
pebble build
pebble install --emulator emery
mkdir -p docs/shots
pebble screenshot --no-open --emulator emery docs/shots/00-skeleton.png
```

View the PNG. Expect a brass circle on black. If the emulator shows another app: `pebble kill && pebble wipe`, reinstall.

**Done when:** `pebble build` succeeds for both targets and the screenshot shows the circle.

**Commit:** `step 0.1: scaffold buildable watchface`

### Step 0.2 — baseline commit and host test runner skeleton

- `test/run.sh` exists, is executable, and currently prints `no tests yet` and exits 0.
- `README.md` documents: `pebble build`, `pebble install --emulator emery`, `pebble screenshot`, `./test/run.sh`, and that `pebble package install @rebble/clay` is needed after a fresh clone (from phase 6).
- Commit everything including `SPEC.md` and `PLAN.md`.

**Commit:** `step 0.2: test runner skeleton and docs`

---

# Phase 1 — fixed point and trig

### Step 1.1 — `fixmath`

**Test first** — `test/test_fixmath.c`:

- `q16_mul(65535, 65535) == 65534` (or within 1), `q16_mul(-65535, 65535) == -65534`, `q16_mul(0, x) == 0`.
- `clamp_i32` at the edges and outside.
- `cdeg_to_trig(9000) == 16384`, `cdeg_to_trig(-9000) == -16384`, `cdeg_to_trig(5148) == 9372` (±1).
- `e4_to_trig(900000) == 16384`, `e4_to_trig(-900000) == -16384`, `e4_to_trig(514779) == 9372` (±1).
- `isqrt_u64(0)==0`, `1==1`, `2==1`, `3==1`, `4==2`, `1024*1024==1024`, `(1024*1024+1)==1024`, `(65535ull*65535ull)==65535`.
- `q16_mul` must not overflow: test `q16_mul(65535, 65535)` and check the result is not negative.

**Implement** `src/c/fixmath.h/.c`:

```c
int32_t q16_mul(int32_t a, int32_t b);       /* (int64)a*b / 65536 */
int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi);
int32_t cdeg_to_trig(int32_t cdeg);          /* 0.01 deg -> TRIG */
int32_t e4_to_trig(int32_t e4);              /* 1e-4 deg -> TRIG */
uint32_t isqrt_u64(uint64_t v);              /* floor sqrt, Newton */
```

**Verify:** `./test/run.sh` compiles and runs `test_fixmath` only.

**Done when:** all asserts pass, `-Wall -Wextra -Werror` clean.

**Commit:** `step 1.1: fixed-point helpers`

### Step 1.2 — `trig` wrapper

**Test first** — `test/test_trig.c` (add to the runner):

- `tri_sin(0) == 0`
- `tri_sin(16384) == 65535` (`TRIG_MAX_RATIO`)
- `tri_sin(32768) == 0` (±1)
- `tri_sin(49152) == -65535`
- `tri_cos(0) == 65535`
- `tri_cos(8192) == 46341` (±2) — 45°
- odd symmetry: `tri_sin(-a) == -tri_sin(a)` for several `a`.

**Implement**

- `src/c/trig.h`:

```c
int32_t tri_sin(int32_t trig_angle);
int32_t tri_cos(int32_t trig_angle);
```

- `src/c/trig.c`: include `<pebble.h>`, delegate to `sin_lookup` / `cos_lookup`.
- `test/shim/trig_host.c`: include `<math.h>`, use `llround(sin(angle * 2π / 65536) * 65535)` and the same for cos. **No shared code with the watch implementation.**

**Verify:** `./test/run.sh`.

**Commit:** `step 1.2: trig abstraction with host shim`

---

# Phase 2 — solar model

### Step 2.1 — solar table generator

**Test first** — `test/test_solar_table.py`:

- Import `tools/gen_solar_table.py`, call `solar_day(index)`.
- Exactly 366 days, index 59 is Feb 29.
- Regression anchors at 00:00 UTC (tolerance 0.02° / 0.05 min):

| Date (2028, leap) | index | decl cdeg | EoT cmin |
|---|---|---|---|
| 2028-01-01 | 0 | −2305 | −312 |
| 2028-03-20 | 79 | −4 | −748 |
| 2028-06-21 | 172 | 2343 | −186 |
| 2028-12-21 | 355 | −2343 | 191 |

- Declination stays within ±2350 cdeg; EoT within −1500..+1700 cmin.
- Day-to-day declination change never exceeds 50 cdeg.
- `python3 tools/gen_solar_table.py --check` exits 0 and the committed header matches a fresh generation.

**Implement** `tools/gen_solar_table.py`:

- NOAA low-precision solar position, no network, Python stdlib only. Generate the table for **2028** (a leap year, so all 366 date slots are real).
- Use exactly this model (day index 0 = 2028-01-01 00:00 UTC):

```
JD   = unix_seconds / 86400 + 2440587.5
n    = JD - 2451545.0
L    = mod(280.460 + 0.9856474*n, 360)
g    = radians(mod(357.528 + 0.9856003*n, 360))
λ    = radians(mod(L + 1.915*sin(g) + 0.020*sin(2g), 360))
ε    = radians(23.439 - 0.0000004*n)
δ    = degrees(asin(sin(ε)*sin(λ)))
RA   = degrees(atan2(cos(ε)*sin(λ), cos(λ)))
EoT  = 4 * wrap180(L - 0.0057183 - RA)      # minutes
```

- Round to `int16`: `decl_cdeg = round(δ*100)`, `eot_cmin = round(EoT*100)`.
- Functions: `solar_day(day_index) -> (decl_cdeg, eot_cmin)` and `generate() -> str`.
- `--check` compares generated text with `src/c/solar_table.h`; `--write` writes it.
- Header:

```c
#define SOLAR_TABLE_DAYS 366
static const int16_t k_decl_cdeg[SOLAR_TABLE_DAYS] = { ... };
static const int16_t k_eot_cmin[SOLAR_TABLE_DAYS] = { ... };
```

- Include a comment with the generation date and model.

**Verify:** `python3 test/test_solar_table.py && python3 tools/gen_solar_table.py --check`.

**Commit:** `step 2.1: generated solar declination/EoT table`

### Step 2.2 — solar state (AST, declination, hour angle)

**Test first** — `test/test_solar.c`:

- Anchor cases, tolerance 0.1° / 0.2 min:

| UTC | lat | lon | δ | EoT | AST | H |
|---|---|---|---|---|---|---|
| 2026-06-21 12:00 | 51.4779 | −0.0015 | +23.435 | −1.85 | 11:58.1 | −0.47° |
| 2026-12-21 12:00 | 51.4779 | −0.0015 | −23.435 | +1.91 | 12:01.9 | +0.47° |
| 2026-03-20 09:00 | 51.4779 | −0.0015 | −0.093 | −7.52 | 8:52.4 | −46.9° |
| 2026-06-21 02:00 | −33.8688 | 151.2093 | +23.435 | −1.76 | 12:03.0 | +0.75° |

- Longitude sign: +1° east adds exactly 4 minutes; −1° west subtracts 4.
- Wrap: 23:59 and 00:01 stay in `[0, 144000)`.
- Leap day: 2028-02-29 uses index 59; 2028-03-01 uses index 60; 2026-03-01 also uses index 60 (leap table, date-indexed).
- Year wrap: 2026-12-31 23:59 indexes 365 and interpolates toward index 0 without reading out of bounds.
- Time-zone independence: call `setenv("TZ", "Pacific/Kiritimati", 1); tzset();` before and after, same outputs.
- `hour_angle` in TRIG units: pick a longitude that makes AST land exactly on the hour (`lon_e4 = -eot_cmin*25`, rounded) and assert 06:00 = −16384 (±2), 18:00 = +16384 (±2), 12:00 = 0 (±2).

**Implement** `src/c/solar.h/.c`:

```c
typedef struct {
  int16_t decl_cdeg;
  int16_t eot_cmin;
  int32_t ast_cmin;     /* 0..143999 */
  int32_t hour_angle;   /* TRIG units, -32768..32767 */
} SolarState;

void solar_state(time_t utc, int32_t lon_e4, SolarState *out);
```

- Use `gmtime`, never `localtime`.
- `idx = month_start_leap[tm_mon] + tm_mday - 1`, `next = (idx + 1) % 366`.
- `frac_q16 = ((int64_t)sod << 16) / 86400`.
- `ast_cmin = sod*100/60 + lon_e4/25 + eot_cmin`, wrapped to `[0,144000)`.
- `hour_angle = (int64_t)(ast_cmin - 72000) * 65536 / 144000`.

**Verify:** `./test/run.sh`.

**Done when:** all anchors pass and `grep -rn "localtime" src/c` is empty.

**Commit:** `step 2.2: solar state and apparent solar time`

### Step 2.3 — sun vector

**Test first** — extend `test/test_solar.c`:

- For each anchor case, compute the expected `(x,y,z)` in `double` from the C `decl_cdeg` and `hour_angle` using the formulas in `SPEC.md` §3.2. Tolerance 8/65535 per component.
- Unit length: `x²+y²+z²` within 12/65535 of `65535²`.
- Noon altitude: `asin(z)` matches the spec alt (61.95°, 15.09°, 25.12°, 32.69°) within 0.3°.
- At solar noon, `x` ≈ 0 and the sun is on the equator side of the zenith: `y·sign(lat) < 0` (so the shadow points at the visible pole).
- Sydney at 9am solar: shadow direction (see phase 3) points up-right; defer the exact assert to step 3.3.
- Night: at London 2026-06-21 23:00, `z < 0`.

**Implement** in `src/c/solar.c`:

```c
typedef struct { int32_t x, y, z; } Vec3q16;   /* Q16.0, x east, y north, z up */

Vec3q16 sun_vector(const SolarState *s, int32_t lat_e4);
```

Use `tri_sin`/`tri_cos` and `q16_mul`. Compute with `int64` intermediates. Convert lat via `e4_to_trig`, decl via `cdeg_to_trig`.

**Verify:** `./test/run.sh`.

**Commit:** `step 2.3: sun vector in local coordinates`

---

# Phase 3 — dial geometry

### Step 3.1 — latitude clamp and gnomon

**Test first** — `test/test_dial.c`:

- `dial_init` clamps: lat 0 → 15°, 10° → 15°, 15° → 15°, 51.4779° unchanged, 75° unchanged, 89° → 75°.
- Southern latitudes use `pole = -1` and the same absolute geometry.
- At 51.4779°: `h_q10 == 532` (±1), `L_q10 == 423` (±2), `w_q10 == 61`.
- At 25°: `L_q10 == 737` (±2), `h_q10 == 344` (±2).
- At 75°: `h_q10 == 532`, `L_q10 == 142` (±2).
- No division by zero at any clamp boundary; `h_q10 > 0`, `L_q10 > 0`.

**Implement** `src/c/dial.h/.c`:

```c
typedef struct {
  int32_t pole;           /* +1 north, -1 south */
  int32_t lat_abs_e4;     /* clamped to 150000..750000 */
  int32_t h_q10, L_q10, w_q10;
  int32_t sin_lat, cos_lat;   /* Q16 of clamped |lat| */
  PointQ10 hour_dir[13];      /* local shadow direction, |v| ~= 1024 */
} Dial;

void dial_init(Dial *d, int32_t lat_e4);
```

Use `sin_lat/cos_lat` from `tri_sin/tri_cos`; compute `tan = sin/cos` with int64, cap `h`, then `L = h*cos/sin`.

**Commit:** `step 3.1: latitude clamp and gnomon dimensions`

### Step 3.2 — hour ring

**Test first** — extend `test/test_dial.c`:

- At 51.4779°, for hours 6..18, convert `hour_dir[i]` to a clockwise-from-up angle using `atan2(x, -y)` (y is north, convert to screen first with `pole=+1`). Compare to:

```
[270.0, 288.9, 306.4, 322.0, 335.7, 348.2, 0.0, 11.8, 24.3, 38.0, 53.6, 71.1, 90.0]
```

Tolerance 0.15°.

- At 0° the ring uses the clamped 15° geometry: the directions are finite and match a dial initialised at 15°.
- Southern ring is the mirror of the northern ring at the same `|lat|`: `screen_dir(SH, h).x == -screen_dir(NH, h).x` and `.y` equal.
- `dial_to_screen` with `pole=+1` returns `(x,-y)`; with `pole=-1` returns `(-x,y)`.
- 6am is screen-left in NH and screen-right in SH; 6pm the other way.

**Implement**

```c
PointQ10 dial_to_screen(const Dial *d, PointQ10 local);
```

In `dial_init`, set `hour_dir[i] = normalize(sin|φ|·sinH, pole·cosH)` where `H = -90° + 15°·i` and `φ` is the **clamped** latitude. Use `isqrt_u64` and Q10 scaling.

Sanity anchors (normalise before comparing): at 51.4779°N, 9am maps to screen `(-0.616,-0.788)` (322.0°); at 51.4779°S, 9am maps to `(+0.616,-0.788)` (38.0°). At 6am the local vector is due west in both hemispheres.

**Commit:** `step 3.2: non-uniform hour ring with hemisphere mirroring`

### Step 3.3 — projection, hull, reading edge

**Test first** — extend `test/test_dial.c`:

- `dial_shadow_visible`: false at `uz = 2286`, true at `2287`.
- Six projected points at a normal daytime sun, all finite.
- `convex_hull` returns 3..6 points, in convex order, no duplicates, and every input point lies inside or on the hull (cross-product test, tolerance 1 Q10).
- The style tip projection at solar noon points up (screen angle 0 ± 0.2°) in both hemispheres.
- The style tip direction matches the matching `hour_dir` entry: normalized dot product > 0.9995.
- Reading-edge distance at London winter noon > summer noon > 0.
- Sydney 2026-06-21 02:00: screen shadow angle = 359.6° ± 0.3° (the SH noon check).
- Sydney 9am solar (`H = -45°`, dec 23.435°): screen angle = 29.1° ± 0.5°.
- London 9am solar: screen angle = 322.0° ± 0.5°.

**Implement**

```c
bool dial_shadow_visible(Vec3q16 sun);
void dial_shadow_points(const Dial *d, Vec3q16 sun, PointQ10 out[6]);
PointQ10 dial_style_tip(const Dial *d, Vec3q16 sun);
int convex_hull(PointQ10 in[6], PointQ10 out[6]);
```

- Prism vertices: `(±w/2, 0, 0)`, `(±w/2, pole*L, 0)`, `(±w/2, pole*L, h)`.
- Projection: `p' = p - u*(p.z/u.z)`, with `int64` and the Q10 scale. `dial_shadow_points` and `dial_style_tip` must return immediately when `!dial_shadow_visible(sun)` (zero the outputs); never divide by a near-zero `u.z`.
- Monotone chain hull, cross product in `int64`.

**Commit:** `step 3.3: gnomon shadow projection and hull`

### Step 3.4 — penumbra band scale

**Test first** — extend `test/test_dial.c`:

- `band_scale_q16(max_dist_q10, offset_px_q10)` returns 65536 when offset is 0.
- Monotonic in offset; offset 0..6 px gives scale 1.0..~1.1 for a 60 px reach.
- Never returns a scale below 65536 or above 131072.

**Implement** in `dial.c`:

```c
int32_t band_scale_q16(int32_t max_dist_q10, int32_t offset_px_q10);
/* 65536 + offset * 65536 / max(1, max_dist) */
```

**Commit:** `step 3.4: penumbra band scaling`

---

# Phase 4 — theme and dither

### Step 4.1 — scene selection

**Test first** — `test/test_theme.c`:

| uz | theme | shadow |
|---|---|---|
| 65535 | DAY | 3 |
| 6850 | DAY | 3 |
| 6849 | GOLDEN | 2 |
| 4572 | GOLDEN | 2 |
| 4571 | GOLDEN | 1 |
| 2287 | GOLDEN | 1 |
| 2286 | GOLDEN | 0 |
| 0 | GOLDEN | 0 |
| −1144 | GOLDEN | 0 |
| −1145 | NIGHT | 0 |
| −65535 | NIGHT | 0 |

**Implement** `src/c/theme.h/.c`:

```c
typedef enum { THEME_DAY, THEME_GOLDEN, THEME_NIGHT } Theme;
typedef struct { Theme theme; uint8_t shadow; } Scene;
Scene scene_for_sun(int32_t uz_q16);
```

Constants: `SIN6_Q16 = 6850`, `SIN4_Q16 = 4572`, `SIN2_Q16 = 2287`, `SIN_M1_Q16 = −1144` (night starts at −1°, per spec §3.5).

**Commit:** `step 4.1: day/golden/night scene selection`

### Step 4.2 — Bayer stipple

**Test first** — `test/test_theme.c`:

- A 4×4 tile of `bayer4(x,y)` contains every value 0..15 exactly once.
- `bayer4` is deterministic and repeats with period 4 in both axes.
- `stipple_on(x, y, density)` is true for exactly `density` of 16 cells per tile (density 0..16).

**Implement** in `theme.c`:

```c
uint8_t bayer4(int32_t x, int32_t y);
bool stipple_on(int32_t x, int32_t y, uint8_t density);
```

Use the standard 4×4 Bayer matrix, anchored to screen coordinates.

**Commit:** `step 4.2: deterministic 4x4 Bayer dither`

---

# Phase 5 — rendering

**Rules for this phase**

- All geometry comes from phases 1–4. Do not put math in `render.c` beyond layout conversion.
- Use `graphics_context_set_antialiased(true)` for circles, `false` for 1 px lines and text.
- One full redraw per minute; no timers, no animation.
- After each step: build, install, screenshot, view the PNG, then run the probe.

### Step 5.0 — screenshot probe

**Create** `test/check_shot.py` (PIL is installed):

```
python3 test/check_shot.py IMAGE --center CX,CY --radius R \
    --shadow ANGLE_DEG [--r0 0.45 --r1 0.60] [--wedge 3] \
    --expect day|night [--max-center-lum N]
```

- Compute mean luminance of pixels in a ±3° wedge around `ANGLE_DEG` (clockwise from up) between `r0*R` and `r1*R`, and the same wedge at `ANGLE_DEG+180`.
- `--expect day`: wedge luminance must be < 0.80 × opposite wedge.
- `--expect night`: wedge and opposite must be within 15% of each other.
- `--max-center-lum N`: assert the mean of the centre 10×10 is ≤ N (use 100 for night).
- Also support `--pixel X,Y --color RRGGBB --tol 24`.
- Exit non-zero with a readable message on failure.

Test it against the step 0.1 screenshot: centre pixel is brass-ish, corner (2,2) is black.

**Commit:** `step 5.0: screenshot luminance probe`

### Step 5.1 — static dial: case, brass, rim

**Implement** in `src/c/render.h/.c`:

- Platform layout constants:

| Constant | emery | gabbro |
|---|---|---|
| `DIAL_CX` | 100 | 130 |
| `DIAL_CY` | 114 | 130 |
| `DIAL_R` | 94 | 122 |
| `RIM_W` | 5 | 6 |
| `NUM_R` | 0.82R | 0.82R |
| `MOTTO_Y` | +0.63R | +0.63R |

- Draw: black case, brass base circle, 4 concentric bands with Bayer transitions, darker toward the rim, 3 fixed patina stipples near the rim, rim band (ArmyGreen/DarkGray), 1 px highlight arc.
- Use the coordinates from §2 of `SPEC.md`. No numerals, lines, blade or shadow yet.

**Verify:** screenshot `docs/shots/51-static-dial-emery.png`; probe centre brass, corner black. Compare with the spec's palette table.

**Commit:** `step 5.1: brass dial and case`

### Step 5.2 — engraving: hour lines, numerals, motto, hub, pole index

- 13 hour lines from `0.14R` to `0.70R`, using `hour_dir` + `dial_to_screen`, 1 px (2 px on gabbro). VI at both sides, XII at top.
- Draw a dark line then a 1 px light line offset `(+1,+1)` to fake engraving.
- Numerals: vector Roman glyphs (I/V/X strokes) drawn with 1 px strokes (2 px on gabbro), engraved two-tone, upright, labels `VI, VII, VIII, IX, X, XI, XII, I, II, III, IV, V, VI` at `0.82R`, centred. Place VI twice (6am and 6pm). Each glyph is a small array of line segments; draw dark core, then 1 px light offset `(+1,+1)`. If the glyphs look crude in the emulator, fall back to `FONT_KEY_GOTHIC_14` (emery) / `FONT_KEY_GOTHIC_18` (gabbro) and record it in `DECISIONS.md`.
- Motto at `(cx, cy + 0.63R)`: `HORAS NON NUMERO NISI SERENAS` in `FONT_KEY_GOTHIC_09` (emery) / `FONT_KEY_GOTHIC_14` (gabbro). Measure with `graphics_text_layout_get_content_size`; if wider than the dial chord at that y, use `TEMPUS FUGIT`.
- Hub circle at centre, `0.12R`.
- Pole index dot at `0.97R` above centre, filled.

**Verify:** screenshot; check no numeral overlaps, all inside the rim, motto inside the circle. Adjust `NUM_R` between 0.80 and 0.88 and the font only if needed; record the final values in `DECISIONS.md`.

**Commit:** `step 5.2: engraved ring, numerals, motto`

### Step 5.3 — gnomon blade

- Footprint: axis-aligned rectangle from the centre upward, width `w`, length `L` (always up in screen coordinates).
- Ridge line from centre to the far end, 1 px, ChromeYellow on the sunward side.
- Bevel highlight on the left or right edge depending on the sign of the sun's screen x.
- 2 px contact shadow across the base; rim light on the far edge.
- Night palette handled in step 5.5.

**Verify:** screenshot; blade points up, is centred, does not exceed `L`.

**Commit:** `step 5.3: gnomon blade`

### Step 5.4 — cast shadow

- Wire the live face in `main.c`: keep a `Location` (hard-coded Greenwich until phase 6), a `Dial`, and a `SolarState`; subscribe with `tick_timer_service_subscribe(MINUTE_UNIT, tick_cb)`; on init and on every tick call `solar_state(time(NULL), loc.lon_e4, &solar)`, `sun_vector`, `dial_init` after a location change, then `layer_mark_dirty`. Unsubscribe in `deinit`.
- For the current sun: `scene_for_sun` → if `shadow > 0`, project the six prism points and hull them.
- Draw bands from outside in: outer at `band_px`, mid at `band_px/2`, core at `0`, scaled about the centre using `band_scale_q16`.
- `band_px = 2 + max_dist_px/50`, capped at 6 px on emery and 8 px on gabbro.
- Fill each band with a scanline polygon fill; clip each row to the dial circle with `isqrt_u64`.
- Core density 12/16 (Black + ArmyGreen), mid 8/16, outer 4/16. Bayer pattern anchored to screen.
- Draw the crisp reading edge from centre to `dial_style_tip` after the fill, 1 px, near-black. There is no clip API: clamp the endpoint to `0.97R` so a long winter/sunrise shadow does not draw on the case. The fill itself is row-clipped to the circle.
- Draw the blade after the shadow.

**Verify** with three screenshots:

```
pebble emu-set-time 1782043200 --emulator emery   # summer noon
pebble install --emulator emery
pebble screenshot --no-open --emulator emery docs/shots/54-summer-noon.png
python3 test/check_shot.py docs/shots/54-summer-noon.png --center 100,114 --radius 94 --shadow 0 --expect day
```

Repeat for winter noon (`1797854400`, angle 0) and equinox 09:00 (`1773997200`, angle 320.1). The summer shadow must be visibly shorter than the winter shadow.

**Commit:** `step 5.4: soft-edged cast shadow`

### Step 5.5 — golden, night, uncalibrated

- Palette switch by `Scene.theme` exactly per `SPEC.md` §4.1. Keep a single `Palette` struct.
- Night: no shadow, dim blade silhouette with a cool rim, faint engraving, dial visible.
- Golden: Warmer base, no umbra, shadow only if level > 0.
- Calibrated flag (passed from `main.c`): when false, engraving is dim (Bayer 50% toward base), pole index hollow.
- No text or vibration for either state.

**Verify**

```
# night
pebble emu-set-time 1782082800 --emulator emery
pebble install --emulator emery
pebble screenshot --no-open --emulator emery docs/shots/55-night.png
python3 test/check_shot.py docs/shots/55-night.png --center 100,114 --radius 94 --shadow 0 --expect night --max-center-lum 100
```

Golden: use `1782072000` (2026-06-21 20:00 UTC, sun about 1.7° above the horizon in London) and confirm the warm palette and fading shadow. Tune the exact timestamp to the visual result and record it.

**Commit:** `step 5.5: golden, night and provisional palettes`

---

# Phase 6 — location and settings

### Step 6.1 — AppMessage inbox and validation

**Test first** — `test/test_location.c`:

- Valid: `(0,0)`, `(900000,1800000)`, `(-900000,-1800000)`, `(514779,-15)`.
- Invalid: lat `±900001`, lon `±1800001`, `INT32_MIN/MAX`.
- Accepts latitude exactly ±90° and longitude exactly ±180°.

**Implement**

- `src/c/location.h/.c`: `bool location_valid(int32_t lat_e4, int32_t lon_e4);`
- `src/c/sundial.h`:

```c
typedef struct {
  int32_t lat_e4, lon_e4;
  uint8_t source;   /* 0 default, 1 manual, 2 GPS */
  bool valid;
} Location;
```

- `main.c`: register inbox callback, then `app_message_open(128,128)`. Handle the five keys. On a valid new location, update `Location`, call `dial_init`, persist, redraw. Ignore invalid messages and keep the old location. On launch, send `REQUEST_LOCATION` once.

**Verify:** build, install, then:

```
pebble send-app-message --emulator emery \
  --int LAT_E4=-338688 --int LON_E4=1512093 \
  --uint LOCATION_SOURCE=1 --uint LOCATION_VALID=1
pebble screenshot --no-open --emulator emery docs/shots/61-sydney.png
```

With the emulator time at `1782043200`, the shadow must point up (near XII) and VI must be on the right.

**Commit:** `step 6.1: AppMessage location intake`

### Step 6.2 — persistence

- Persist `lat_e4`, `lon_e4`, `source`, `valid` on change; load on launch. Default: Greenwich `514779, -15`, source 0, valid false.
- No periodic GPS polling. Request once on launch; request again on `connection_service` connect.
- Test manually: send Sydney, kill the app (or `pebble install` again), relaunch, confirm it still draws Sydney without a phone message. Then `pebble wipe` and confirm the default + provisional state.

**Commit:** `step 6.2: persist location across launches`

### Step 6.3 — pkjs manual location + settings validation

**Test first** — `test/settings.test.mjs` with `node --test`:

- `parseCoord("51.4779", -90, 90) === 514779`
- `parseCoord("-33.8688", -90, 90) === -338688`
- rejects `""`, `"abc"`, `"91"`, `"NaN"`, `Infinity`, and values with >6 decimal places are rounded, not rejected.
- `parseLocation("51.4779", "0")` returns `{lat_e4, lon_e4}`; invalid input returns `null`.
- Rounding: `parseCoord("51.47799")` returns `514780` (not a truncation).

**Implement** `src/pkjs/settings.js` (CommonJS; exported for Node, required by `index.js`). In `index.js`:

- On `ready`: if `useGps`, request a fix; else send the stored manual coordinate.
- On `REQUEST_LOCATION`: same, throttled to one request per 10 minutes.
- On fix success: `LAT_E4 = Math.round(lat*10000)`, clamp, send; source 2.
- On failure/denied: send manual; source 1.
- `localStorage` keys: `useGps`, `lat`, `lon`.
- No Clay yet; defaults are London.

**Verify:** `node --test test/`; in the emulator, `pebble emu-set-location 51.5 -0.1 --emulator emery` and confirm the face updates. Also verify with `send-app-message` that a manual message wins.

**Commit:** `step 6.3: phone-side location and validation`

### Step 6.4 — GPS capability and Clay

- `pebble package install @rebble/clay` (adds `dependencies`). Commit `package.json`.
- `src/pkjs/config.js`: toggle `useGps`; text inputs `lat`, `lon`; a `defaultValue` for each; an informational `text` block with the spec's wording (apparent solar time, ±16 min, 15°–75°, no digital time).
- `index.js`: `clay` config, `webviewclosed` handler, save to `localStorage`, send the manual location immediately if GPS is off, otherwise request a fresh fix.
- Add the three listeners explicitly: `ready`, `showConfiguration` → `Pebble.openURL(clay.generateUrl())`, `webviewclosed` → `clay.getSettings(e.response, false)`.
- Reject invalid coordinates in the handler and keep the previous values.
- Verify: `pebble emu-app-config --emulator emery` opens the page; change the coordinates; watch the face move. Also build both platforms.

**Commit:** `step 6.4: Clay settings page`

### Step 6.5 — provisional and out-of-range states

- `location_valid` range is ±90/±180, but the dial's accurate band is 15°–75°. Below/above it, `dial_init` clamps and `main.c` marks the ring provisional (same visual treatment as uncalibrated).
- The pole index is filled only for a valid location inside the accurate band.
- Test by sending `LAT_E4=50000` (5°): the face still draws, the ring is dim, no crash, and the hour lines are clamped. Send `LAT_E4=850000` (85°): same.
- Document the behavior in `DECISIONS.md`.

**Commit:** `step 6.5: provisional state for out-of-band latitudes`

---

# Phase 7 — gabbro, QA, release

### Step 7.1 — gabbro layout

- Add the gabbro constants from step 5.1; font sizes and stroke widths per the layout table.
- `pebble build`; `pebble install --emulator gabbro`; screenshots for summer noon, winter noon, night.
- Probe with `--center 130,130 --radius 122`.
- Check the round screen: nothing clipped, motto inside, pole dot inside.

**Commit:** `step 7.1: gabbro layout`

### Step 7.2 — acceptance matrix and soak

Run and save every shot under `docs/shots/`:

| Shot | Time (UTC) | Location | Expect |
|---|---|---|---|
| emery summer noon | 1782043200 | London | short shadow at XII |
| emery winter noon | 1797854400 | London | long shadow at XII |
| emery equinox 09:00 | 1773997200 | London | shadow at ~320° |
| emery night | 1782082800 | London | no shadow, dim dial |
| emery Sydney noon | 1782043200 + Sydney msg | Sydney | shadow up, VI right |
| emery provisional | any | default / wipe | dim ring, hollow index |
| gabbro summer noon | 1782043200 | London | as above |
| gabbro night | 1782082800 | London | as above |

Repeat the emery summer-noon and night shots with `--no-correction` and run the same probes; contrast must hold in both modes.

Also:

- Set two different emulator time zones (e.g. `TZ=UTC` vs `TZ=Asia/Tokyo` in the shell that launches the emulator) and confirm the shadow is identical.
- `pebble logs` started before install; run 24 simulated hours with `emu-set-time` in one-hour steps and confirm no crash, no leak warnings, and no stale screen.
- `pebble analyze-size`; record flash/RAM usage.
- Confirm the 24 h run never divides by zero (watch the logs).

**Commit:** `step 7.2: QA matrix and screenshots`

### Step 7.3 — release assets

- App icon via `~/.config/opencode/skills/pebble-watchface/scripts/create_app_icons.py`.
- App store description: copy the settings-page note (apparent solar time, equation of time, 15°–75°, no digital time by design).
- `pebble build`; confirm `build/*.pbw` exists for emery and gabbro.
- Final read-through of `README.md`.

**Commit:** `step 7.3: release assets`

---

# Appendix A — interfaces (single source)

```c
/* fixmath.h */
int32_t q16_mul(int32_t a, int32_t b);
int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi);
int32_t cdeg_to_trig(int32_t cdeg);
int32_t e4_to_trig(int32_t e4);
uint32_t isqrt_u64(uint64_t v);

/* trig.h */
int32_t tri_sin(int32_t trig_angle);
int32_t tri_cos(int32_t trig_angle);

/* solar.h */
typedef struct {
  int16_t decl_cdeg, eot_cmin;
  int32_t ast_cmin, hour_angle;
} SolarState;

typedef struct { int32_t x, y, z; } Vec3q16;

void solar_state(time_t utc, int32_t lon_e4, SolarState *out);
Vec3q16 sun_vector(const SolarState *s, int32_t lat_e4);

/* dial.h */
typedef struct { int32_t x, y; } PointQ10;

typedef struct {
  int32_t pole;
  int32_t lat_abs_e4;
  int32_t h_q10, L_q10, w_q10;
  int32_t sin_lat, cos_lat;
  PointQ10 hour_dir[13];
} Dial;

void dial_init(Dial *d, int32_t lat_e4);
PointQ10 dial_to_screen(const Dial *d, PointQ10 local);
bool dial_shadow_visible(Vec3q16 sun);
void dial_shadow_points(const Dial *d, Vec3q16 sun, PointQ10 out[6]);
PointQ10 dial_style_tip(const Dial *d, Vec3q16 sun);
int convex_hull(PointQ10 in[6], PointQ10 out[6]);
int32_t band_scale_q16(int32_t max_dist_q10, int32_t offset_px_q10);

/* theme.h */
typedef enum { THEME_DAY, THEME_GOLDEN, THEME_NIGHT } Theme;
typedef struct { Theme theme; uint8_t shadow; } Scene;
Scene scene_for_sun(int32_t uz_q16);
uint8_t bayer4(int32_t x, int32_t y);
bool stipple_on(int32_t x, int32_t y, uint8_t density);

/* location.h */
bool location_valid(int32_t lat_e4, int32_t lon_e4);

/* render.h */
void render_draw(GContext *ctx, GRect bounds, const Dial *dial,
                 const SolarState *solar, Vec3q16 sun, bool calibrated);
```

# Appendix B — constants and reference values

**Trig:** `TRIG_MAX_ANGLE = 65536`, `TRIG_MAX_RATIO = 65535`. All Q16.0.

**Scene:** `SIN2 = 2287`, `SIN4 = 4572`, `SIN6 = 6850`, `SIN(−1°) = −1144`.

**Hour ring at 51.4779°N, clockwise from up:**

```
hour:   6     7     8     9    10    11    12    13    14    15    16    17    18
angle: 270.0 288.9 306.4 322.0 335.7 348.2   0.0  11.8  24.3  38.0  53.6  71.1  90.0
```

**Solar anchors** (see `SPEC.md` §7.1 for the full table): London summer noon shadow 359.6°, winter noon 0.45°, equinox 09:00 320.1°; Sydney winter noon 359.6°, Sydney 9am 29.1°.

**Emulator timestamps**

```
1782043200  2026-06-21 12:00 UTC
1773997200  2026-03-20 09:00 UTC
1797854400  2026-12-21 12:00 UTC
1782082800  2026-06-21 23:00 UTC
```

# Appendix C — traps that will bite

1. **Trig scale is 65536, not 32768.** Use `(int64_t)a * b / 65536` (or an arithmetic `>> 16`). `>> 15` doubles every result. Division keeps negative products symmetric, which the tests assume.
2. **`atan2_lookup` takes `int16_t`.** Avoid it; hour-ring directions are computed by normalization instead.
3. **Southern hemisphere sign.** `hour_dir` uses `sin|φ|` with a pole-signed `cosH`; `dial_to_screen` flips both axes for `pole = -1`. Do not add a separate mirror pass.
4. **Never use sun azimuth + 180°.** The shadow is the projection of the style; only that lands on the hour line.
5. **AppMessage keys must be listed in `package.json` `messageKeys`.** Use `MESSAGE_KEY_LAT_E4`, never hard-coded integers.
6. **Register the inbox callback before `app_message_open`.**
7. **`tick_time` is local time.** Ignore it for solar math; call `time(NULL)` and `gmtime` inside `solar_state`.
8. **`time_t` is 32-bit on the watch.** Fine until 2038; do not build year-2100 logic.
9. **Text vertical centring.** `graphics_draw_text` includes font padding; centre using `graphics_text_layout_get_content_size`, then adjust visually.
10. **1 px lines.** Set antialiasing off; antialiased lines at 1 px look grey and wash out the engraving.
11. **Screenshot directories.** `pebble screenshot` does not create them; `mkdir -p docs/shots` first.
12. **Stale emulator.** Another app or a crash dialog in the screenshot means `pebble kill && pebble wipe` and reinstall. Do not debug your code from someone else's screen.
13. **`emu-set-time` is UTC** and may need an app relaunch before the minute tick fires.
14. **Persist keys are forever.** Pick numbers 1–4 in step 6.2 and never renumber them.
15. **`uz` below `SIN2`:** do not project; the distance explodes toward sunrise/sunset.
16. **No floats.** `strtof`, `float`, and `double` are forbidden in `src/c`, including debug code.
17. **Shadow probe range.** At noon the blade occupies up to `0.41R`; probe `0.45R`–`0.60R` so the blade is not measured instead of the shadow.

# Appendix D — command sheet

```sh
# build and run
pebble build
pebble install --emulator emery
pebble install --emulator gabbro
pebble screenshot --no-open --emulator emery docs/shots/x.png

# host tests
./test/run.sh
node --test test/
python3 test/test_solar_table.py
python3 tools/gen_solar_table.py --check

# deterministic scenarios
pebble emu-set-time 1782043200 --emulator emery
pebble send-app-message --emulator emery \
  --int LAT_E4=-338688 --int LON_E4=1512093 \
  --uint LOCATION_SOURCE=1 --uint LOCATION_VALID=1
pebble emu-set-location 51.5 -0.1 --emulator emery

# logs (start before install)
pebble logs --emulator emery > /tmp/opencode/pebble-emery.log 2>&1 &

# reset
pebble kill && pebble wipe
```

# Appendix E — definition of done

- [ ] `./test/run.sh` and `node --test test/` pass from a clean checkout.
- [ ] `grep -rnE '\b(float|double)\b' src/c` is empty.
- [ ] `pebble build` produces a PBW for emery and gabbro.
- [ ] All eight screenshots in step 7.2 exist and pass their probes.
- [ ] Summer noon shadow is shorter than winter noon; both are on XII.
- [ ] Sydney draws the shadow up at noon and VI on the right.
- [ ] Night has no shadow and no digital time.
- [ ] Uncalibrated and out-of-band states are visibly dim with a hollow index.
- [ ] The shadow stays correct after changing the emulator time zone.
- [ ] A 24-hour simulated run has no crash, no divide-by-zero, no leaked windows.
