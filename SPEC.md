# Sundial — watch face specification

**Status:** draft 1 · **Target:** Pebble Time 2 (emery) + Pebble Round 2 (gabbro)

**Concept:** a plan-view horizontal garden sundial in aged brass. The shadow is computed from the real sun at the wearer's location; it is the only time display. No digital clock, date, battery, Bluetooth icon, or notifications.

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Models | Emery (200×228) + Gabbro (260×260) |
| Location | Phone GPS with manual lat/lon override |
| Readout | Fixed apparent-solar dial only; no civil time |
| Look | Aged brass, engraved Roman numerals, dark soft-edged shadow |
| Language | C (recommended); offline-generated solar table, integer runtime |

**Reading model:** the dial simulates a perfectly level horizontal dial whose noon line points at the visible celestial pole. The screen is therefore always drawn as if correctly aimed — you do **not** hold the watch north. This is a virtual instrument, not a sensor; clouds don't remove the shadow, night does.

## 2. Composition and layout

Circular dial on a black case. Emery uses the rectangle's extra height as breathing room; Gabbro fills the round screen.

```
        pole index
           XII
      XI        I
   X     blade     II
  IX    shadow      III
  VI ──────+────── VI
          motto
```

| Metric | Emery | Gabbro |
|---|---|---|
| Screen | 200×228 | 260×260 round |
| Dial center | (100,114) | (130,130) |
| Dial radius R | 94 px | 122 px |
| Rim band | 5 px | 6 px |
| Hour lines | 1 px, from 0.14R to 0.70R | 2 px, same fractions |
| Noon/substyle line | 2 px | 3 px |
| Numeral ring | 0.82R, 12 px tall | 0.82R, 16 px tall |
| Numeral stroke | 1 px | 2 px |
| Motto | Gothic 9, y = +0.63R | Gothic 14, y = +0.63R |
| Hub | 0.12R | 0.12R |
| Pole index | 3 px dot at 0.97R | 4 px dot at 0.97R |
| Blade width | 6 px | 8 px |
| Shadow bands | 3/2/2 px | 4/3/3 px |

- Hour lines run **6:00–18:00 only** (13 lines). VI sits at left/right, XII at top. The lower half is free for the motto.
- The hour scale is **non-uniform** — roughly `[270, 288.9, 306.4, 322.0, 335.7, 348.2, 0, 11.8, 24.3, 38.0, 53.6, 71.1, 90]°` clockwise from up at 51.48°N. This is correct, not a bug: it is not a clock face.
- Numerals are vector Roman glyphs (I/V/X strokes), upright, engraved two-tone. If they look crude in testing, fall back to the system Gothic font.
- Motto: **HORAS NON NUMERO NISI SERENAS** ("I count only the sunny hours"). Fallback `TEMPUS FUGIT` if it doesn't fit at 9 px on emery.
- The top rim carries a small **pole index** (N above the equator, S below). Filled = location calibrated; hollow = default/provisional. It doubles as an orientation cue without text.

**Layers, bottom to top:** case → brass dial bands/patina → rim + pole index → engraved hour lines, numerals, hub, motto → cast shadow → blade/contact shadow.

## 3. The sundial model

### 3.1 Time

The dial shows **apparent solar time** — what a real sundial reads. It deliberately ignores time zone and DST.

```
AST_minutes = UTC_minutes + 4 * longitude_deg + EoT_minutes   (wrap 0..1440)
H = 15° * (AST_minutes / 60 - 12)                              (hour angle)
```

EoT = apparent minus mean solar time, roughly −14 to +16 minutes. The reading will differ from the phone by up to ~16 min plus the time-zone/longitude offset. This is authenticity, not error.

### 3.2 Solar position

Offline-generated 366-entry table, 0.01° declination and 0.01 min EoT per UTC day, linearly interpolated by day fraction. ~1.5 KB flash, no runtime float math, highly testable.

```
u_x = -cosδ sinH
u_y =  sinδ cosφ - cosδ sinφ cosH
u_z =  sinδ sinφ + cosδ cosφ cosH
```

`u_z > 0` means the sun is up. The table is generated with a standard low-precision NOAA algorithm; host tests compare it to reference values.

### 3.3 Screen mapping

Local axes: x east, y north, z up; screen y grows down.

- Northern hemisphere: `screen = (x, -y)`
- Southern hemisphere: `screen = (-x, y)`

The top of the screen is always solar noon / the visible pole. The southern-hemisphere hour ring is therefore mirrored east-west, with VI on the right at 6am and VI on the left at 6pm. Same composition, correct instrument.

### 3.4 Hour ring

For latitude φ and solar hour angle H:

```
tan θ = sin|φ| * tan H
```

θ measured clockwise from the top; morning negative, afternoon positive. The ring is date-independent. Precompute 13 unit vectors at load (or 15 if summer marks are added later). Reference angles at |φ| = 51.4779° are listed above.

### 3.5 Gnomon and shadow

The gnomon is a thin vertical triangular prism in the meridian plane; its style (hypotenuse) is parallel to Earth's axis and points at the visible pole. Its foot is the dial center.

```
φ_abs = clamp(|latitude|, 15°, 75°)     // geometry range, see 3.7
h = min(0.52R, 0.72R * tan(φ_abs))     // style height
L = h / tan(φ_abs)                      // footprint toward the pole
w = 0.06R                               // plate thickness
```

Prism vertices in local coordinates (p = sign of latitude):

```
(±w/2, 0, 0)
(±w/2, pL, 0)
(±w/2, pL, h)
```

**Cast shadow = convex hull of the six points projected onto the dial plane:**

```
p' = (x - ux*z/uz,  y - uy*z/uz)
```

Do **not** use the sun's azimuth + 180°. That is the shadow of a vertical pin, not a sundial, and it will not align with the hour lines. The 3D projection of the style is what makes the shadow land on the correct hour line.

- The **sharp edge** of the hull along the style's shadow is the reading edge; draw it as a crisp 1 px line over the fill. The rest of the hull is the body/penumbra.
- Shadow is clipped to the dial circle.
- Penumbra width scales with shadow length: `2 px + 2% of reach`, capped at 6 px (emery), ×1.3 on gabbro.
- Shadow length naturally varies: short at summer noon, long at winter noon, enormous near sunrise/sunset.

**Visibility/fade:**

- `u_z ≥ sin 6°`: full shadow.
- `sin 2° ≤ u_z < sin 6°`: golden palette, shadow fades in steps to 0.
- `u_z < sin 2°`: no cast shadow.
- `u_z ≤ −1°` (sun below horizon): night palette, no shadow.

### 3.6 Southern hemisphere

Compute with the signed latitude and the screen mapping above. At solar noon the shadow points up; at 9am solar it points up-right (Sydney test: 29.1° clockwise from up), the mirror of London. The blade always points up toward the visible pole; the top marker reads S.

### 3.7 Latitude limits

A horizontal dial is ill-conditioned near the equator and extreme near the poles.

- **Accurate range: 15°–75°.** Below 15° the hour lines collapse; above 75° the style is short and shadows extreme.
- Outside that band, clamp the geometry to ±15°/±75°, keep the dial working, mark the ring **provisional** (dimmed like the uncalibrated state), and explain it in settings. A true equatorial dial is a v2 feature, not v1.
- At 51.5°: `h ≈ 0.52R`, footprint `L ≈ 0.414R`; at 25°: `L = 0.72R`.

## 4. Visual system

### 4.1 Palette

| Role | Day | Golden | Night |
|---|---|---|---|
| Case | Black | Black | Black |
| Dial base | Brass + ChromeYellow stipple | Brass + Rajah stipple | DarkGray + CadetBlue stipple |
| Dial edge | Brass/ArmyGreen | WindsorTan/ArmyGreen | OxfordBlue/DarkGray |
| Engraving | ArmyGreen + ChromeYellow highlight | WindsorTan + Rajah highlight | Black + LightGray highlight |
| Shadow core | 75% Black + 25% ArmyGreen | lighter, no umbra | — |
| Penumbra | 50% ArmyGreen + 50% Brass | 25% ArmyGreen + 75% Brass | — |
| Blade lit / shaded | Rajah / WindsorTan, ChromeYellow top | warmer | DarkGray/Black, LightGray rim |
| Motto | ArmyGreen + Rajah highlight | warm | LightGray, dim |

All colors are candidates from the 64-color palette; expect one visual-tuning pass on the emulator. Use a stable 4×4 Bayer pattern anchored to screen coordinates, never a random pattern per frame.

### 4.2 Metal and engraving

- 3–4 concentric brass bands with 2 px dithered transitions; slightly darker toward the rim.
- 2–4 fixed patina patches near the rim (stippled, low contrast); no per-frame randomness.
- Engraving is a dark core with a 1 px offset highlight below/right — the "cut" illusion.
- A 1 px highlight arc on the rim follows the sun's screen azimuth (subtle, optional).
- Antialiasing on for the dial/rim circles, off for 1 px engraved lines and the numeral strokes.

### 4.3 Shadow

- Three nested fills scaled about the style foot: umbra, mid-penumbra, outer penumbra. The outer band is stippled; the reading edge is a crisp line.
- The shadow is drawn **over** the engraving as a stipple, not solid black: the lines show through faintly, as they would in real shade. This also keeps the dial readable.
- Blade: tapered strip from center to `L`, beveled; the sunward bevel brightens based on the sun's screen-x sign; 2 px contact shadow at the base.
- Night: blade is a dark silhouette with a faint cool rim light; no shadow; motto faintly visible.

### 4.4 Calibration state

Until a valid GPS fix or manual coordinates arrive:

- Use a default location (Greenwich, 51.4779N, 0.0000E).
- Draw the hour lines/numerals with a dim provisional tone; top index hollow.
- Once calibrated: solid engraving and filled index. No text, no vibration, no prompt.

## 5. Location and settings

### 5.1 Phone side (pkjs)

- `capabilities: ["location"]`; `enableMultiJS: true`.
- On `ready`: if "use GPS" is on, call `navigator.geolocation.getCurrentPosition` with low accuracy (1 km is plenty), 30 s timeout; send lat/lon.
- On watch `REQUEST_LOCATION`: same, throttled to one request per 10 minutes.
- If GPS is off or unavailable: send the manual lat/lon from settings.
- Clay settings: "Use phone location" toggle; latitude and longitude text fields (decimal degrees, negative for S/W); a short note: *"This is a real horizontal sundial: it reads apparent solar time, so it can differ from your clock by up to 16 minutes (equation of time). Best accuracy from 15° to 75° latitude. No digital time by design."*
- Save manual values in `localStorage`; validate lat ∈ [−90,90], lon ∈ [−180,180], reject NaN.

### 5.2 Watch side (C)

AppMessage keys:

| Key | Type | Meaning |
|---|---|---|
| `REQUEST_LOCATION` | uint8 | ask phone for a fix |
| `LAT_E4` | int32 | latitude × 10,000 |
| `LON_E4` | int32 | longitude × 10,000 |
| `LOCATION_SOURCE` | uint8 | 0 default, 1 manual, 2 GPS |
| `LOCATION_VALID` | uint8 | 1/0 |

- Register the inbox callback before `app_message_open(128,128)`.
- Persist lat/lon/source/valid with `persist_*`; load on launch so the face works with no phone.
- Request a fix once after launch and again on phone reconnect (if `connection_service` is used).
- On a valid location: recompute the hour ring and gnomon geometry, persist, redraw.
- Invalid/out-of-range message: ignore, keep the last valid location.
- No periodic GPS polling.

## 6. Code architecture and performance

```
src/c/main.c        lifecycle, tick, AppMessage, persistence
src/c/solar.c/.h    table interpolation, AST, sun vector, day/night state
src/c/dial.c/.h     latitude clamping, hour ring, gnomon prism, projection, hull
src/c/render.c/.h   palettes, dial, engraving, numerals, shadow fill, blade
src/pkjs/index.js   GPS + Clay + AppMessage
tools/gen_solar_table.py
test/solar_test.c   assert-based host tests + sin_lookup shim
```

- Runtime: integer/fixed-point only. Q15 trig, Q10 dial units (1024 = R), int32/int64 intermediates, no floats in shipped C.
- `tick_timer_service_subscribe(MINUTE_UNIT, ...)`; no `app_timer` in steady state; no per-frame allocation; static arrays for the 13 hour vectors and 6 shadow vertices.
- Draw order as in §2; one full-face redraw per minute or on location change. Budget: under ~30 ms per redraw on emery; use run-length fills and cap stipple calls.
- No seconds, no animation, no button/touch handling.
- The host tests compile `solar.c`/`dial.c` with a shim for `sin_lookup`/`cos_lookup`, so the math is verified before UI work.

## 7. QA and acceptance

### 7.1 Host tests

Reference cases (UTC, lat, lon):

| Case | δ | EoT | AST | Alt | Shadow clockwise from up |
|---|---|---|---|---|---|
| London 2026-06-21 12:00 | +23.435° | −1.85 min | 11:58.1 | 61.95° | 359.6° |
| London 2026-12-21 12:00 | −23.435° | +1.91 min | 12:01.9 | 15.09° | 0.45° |
| London 2026-03-20 09:00 | −0.093° | −7.52 min | 8:52.4 | 25.12° | 320.1° |
| Sydney 2026-06-21 02:00 | +23.435° | −1.76 min | 12:03.0 | 32.69° | 359.6° (SH mapping) |

Also assert:

- Hour line angles match the 51.4779° table above within 0.1°.
- Shadow direction at solar noon points up in both hemispheres.
- Winter shadow is longer than summer shadow at noon.
- Southern ring is the mirror of the northern ring.
- No shadow when `u_z < sin 2°`.
- Changing the watch's time zone does not change the shadow.
- Leap-day table indexing and year wrap.
- Latitude clamp never divides by zero.
- `u` is unit length within rounding error.

### 7.2 Emulator matrix

```
pebble emu-set-time 1782043200 --emulator emery   # 2026-06-21 12:00 UTC
pebble emu-set-time 1773997200 --emulator emery   # 2026-03-20 09:00 UTC
pebble emu-set-time 1797854400 --emulator emery   # 2026-12-21 12:00 UTC
pebble emu-set-time 1782082800 --emulator emery   # 2026-06-21 23:00 UTC
```

Capture emery and gabbro screenshots for: summer noon, equinox 9am, winter noon, night, uncalibrated, and a southern-hemisphere location (set via Clay/manual). Relaunch the face after changing the emulator time so the minute tick picks it up. Test the GPS path separately with a test AppMessage or `emu-set-location`; use Clay for deterministic locations. Run 24 hours for crashes/leaks and check logs.

### 7.3 Visual acceptance

- Shadow edge points at the current solar hour line; the shadow is short at summer noon, long at winter noon, absent at night.
- No clipping on emery or gabbro; all numerals inside the rim.
- The hour scale reads as a sundial, not a clock (6 at the sides, 12 at top).
- Night state is calm and legible, with no fake shadow.
- Uncalibrated/provisional state visibly distinct.
- Contrast holds in both normal and `--no-correction` screenshots.

## 8. Milestones

1. **Math core** — table generator, solar vector, AST, hour ring, gnomon projection, host tests.
2. **Static emery dial** — brass, rim, vector numerals, motto, pole index.
3. **Dynamic shadow** — cast hull, soft bands, reading edge, blade, day/golden/night palettes; visual iteration.
4. **Location** — pkjs GPS, Clay, AppMessage, persistence, uncalibrated state.
5. **Gabbro + QA** — scaled round layout, screenshot matrix, 24 h run, app icon/screenshots for publishing.

## 9. Calls to confirm

1. **Night:** dim moonlit dial with no shadow and no digital fallback (recommended), or a true moon-shadow (adds lunar math), or an optional tiny digital time (against the brief).
2. **Motto:** use the Latin line (recommended) or leave the lower half blank.
3. **Low latitudes:** clamp to 15°–75° with a provisional/quiet warning (recommended), or add a separate equatorial-dial mode in v1.
4. **Language:** C (recommended) or Alloy.
