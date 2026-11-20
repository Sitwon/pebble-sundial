#!/usr/bin/env python3
"""Host checks for the generated solar table and its generator."""
import datetime
import re
import subprocess
import sys

sys.path.insert(0, "tools")
import gen_solar_table as gen

failures = 0


def check(cond, msg):
    global failures
    if not cond:
        print("FAIL:", msg)
        failures += 1


# --- table size ---
header = gen.generate()
decl_values = re.search(r"k_decl_cdeg\[[^]]*\] = \{([^}]*)\}", header)
eot_values = re.search(r"k_eot_cmin\[[^]]*\] = \{([^}]*)\}", header)
check(decl_values is not None, "decl array present in generated header")
check(eot_values is not None, "eot array present in generated header")

decl = [int(x) for x in decl_values.group(1).split(",") if x.strip()]
eot = [int(x) for x in eot_values.group(1).split(",") if x.strip()]
check(gen.SOLAR_TABLE_DAYS == 366, "exactly 366 days")
check(len(decl) == 366 and len(eot) == 366, "arrays have 366 entries")

# index 59 must be Feb 29 (2028 is a leap year)
check(gen.date_of_index(59) == datetime.date(2028, 2, 29), "index 59 = 2028-02-29")
check(gen.date_of_index(60) == datetime.date(2028, 3, 1), "index 60 = 2028-03-01")
check(gen.date_of_index(0) == datetime.date(2028, 1, 1), "index 0 = 2028-01-01")

# --- regression anchors at 00:00 UTC (tolerance 0.02 deg / 0.05 min) ---
ANCHORS = [
    (datetime.date(2028, 1, 1), 0, -2305, -312),
    (datetime.date(2028, 3, 20), 79, -4, -748),
    (datetime.date(2028, 6, 21), 172, 2343, -186),
    (datetime.date(2028, 12, 21), 355, -2343, 191),
]
for date, idx, want_decl, want_eot in ANCHORS:
    check(gen.date_of_index(idx) == date, f"index {idx} = {date}")
    d = gen.solar_day(idx)
    check(abs(d[0] - want_decl) <= 2,
          f"{date} decl {d[0]} ~= {want_decl} (±2 cdeg)")
    check(abs(d[1] - want_eot) <= 5,
          f"{date} eot {d[1]} ~= {want_eot} (±5 cmin)")

# --- physical bounds ---
check(all(abs(x) <= 2350 for x in decl), "declination within ±2350 cdeg")
check(all(-1500 <= x <= 1700 for x in eot), "EoT within -1500..+1700 cmin")

# --- smoothness: day-to-day declination change never exceeds 50 cdeg ---
worst = max(abs(decl[i + 1] - decl[i]) for i in range(365))
check(worst <= 50, f"max day-to-day declination change {worst} <= 50 cdeg")

# --- year wrap: last day interpolates toward day 0, in bounds ---
check(abs(decl[365] - decl[0]) <= 50, "Dec 31 declination near Jan 1")

# --- freshness: committed header matches a fresh generation ---
res = subprocess.run(
    ["python3", "tools/gen_solar_table.py", "--check"],
    capture_output=True, text=True)
check(res.returncode == 0, f"--check exits 0 ({res.stdout}{res.stderr})")

print("test_solar_table:", "ok" if failures == 0 else f"{failures} failure(s)")
sys.exit(1 if failures else 0)
