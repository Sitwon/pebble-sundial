# Sundial — decisions log

Deviations from `SPEC.md` and choices the plan makes within the spec's latitude. `SPEC.md` stays untouched.

1. **Hour-ring test oracle is analytic, not a second ephemeris.** The Python table test checks day-level regression values; `test_solar.c` checks the four hand-verified cases in `SPEC.md` §7.1. This is enough to catch the realistic failure modes.
2. **Numerals are vector Roman glyphs** (I/V/X stroke construction), per the spec. `FONT_KEY_GOTHIC_14` on emery, `FONT_KEY_GOTHIC_18` on gabbro is the fallback, used only if the glyphs look crude in emulator testing.
3. **Theme thresholds (per spec §3.5, unchanged):** day when `uz ≥ sin 6°`, golden when `0 ≤ uz < sin 6°` (shadow fades out below `sin 2°`), night when `uz ≤ −1°` (`uz_q16 ≤ −1144`). Shadow bands: 3 at `uz ≥ sin 6°`, 2 at `≥ sin 4°`, 1 at `≥ sin 2°`, 0 below. There is no band between `−1°` and `0`; night starts exactly at −1°.
4. **Shadow filling** is a scanline fill clipped row-by-row to the dial circle. No polygon/circle clipping code.
5. **No pixel-perfect golden images.** Screenshots are checked with luminance probes (`check_shot.py`) plus review by the agent.
6. **Clay** uses `@rebble/clay` 1.1.0.
7. **Hour-ring sign.** SPEC §3.4 gives the northern-hemisphere magnitude `tanθ = sin|φ|·tanH`. The implementation uses the signed-latitude vector `(sin|φ|·sinH, pole·cosH)`, which yields the exact southern mirror required by §3.3 and §3.6. Do not add a second mirror pass.
8. **Solar table year.** The 366-entry table is generated from 2028 (leap year, so every date slot is real) and used date-indexed for any year. Year-to-year drift is ~0.1° declination and negligible for a sundial; the app never knows the year.
9. **Step 0.1 skeleton** draws a platform-agnostic circle (centred on layer bounds, radius `min(w,h)/2 − 6`) so both emery and gabbro screenshot correctly before the layout constants exist.
