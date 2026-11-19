# Sundial

A Pebble watchface that casts a physically accurate shadow from a brass dial, computed from real solar position at the wearer's location.

## Build

```sh
pebble build
```

Produces `build/*.pbw` for emery (rect) and gabbro (round).

## Install

```sh
pebble install --emulator emery
pebble install --emulator gabbro
```

## Screenshots

```sh
mkdir -p docs/shots
pebble screenshot --no-open --emulator emery docs/shots/shot.png
```

## Host tests (no Pebble SDK needed)

```sh
./test/run.sh          # C math modules
python3 test/test_solar_table.py   # solar table generator
node --test test/             # phone-side settings parsing
python3 tools/gen_solar_table.py --check   # table freshness
```

## Emulator time control

```sh
pebble emu-set-time 1782043200 --emulator emery   # 2026-06-21 12:00 UTC (summer noon)
pebble emu-set-time 1773997200 --emulator emery   # 2026-03-20 09:00 UTC (equinox)
pebble emu-set-time 1797854400 --emulator emery   # 2026-12-21 12:00 UTC (winter noon)
pebble emu-set-time 1782082800 --emulator emery   # 2026-06-21 23:00 UTC (night)
```

## Settings

Install `@rebble/clay` after a fresh clone:

```sh
pebble package install @rebble/clay
```

Open settings from the watchface: `pebble emu-app-config --emulator emery`.

## Stale emulator

If a screenshot shows another app or a crash dialog:

```sh
pebble kill && pebble wipe
```

Then reinstall.
