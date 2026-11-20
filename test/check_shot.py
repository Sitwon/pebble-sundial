#!/usr/bin/env python3
"""Luminance probe for Pebble emulator screenshots.

Usage:
  check_shot.py IMAGE --center CX,CY --radius R --shadow ANGLE_DEG
                 [--r0 0.45] [--r1 0.60] [--wedge 3] --expect day|night
                 [--max-center-lum N] [--pixel X,Y --color RRGGBB] [--tol 24]

Exits 0 when every requested check passes, 1 otherwise.
"""
import argparse
import math
import sys

from PIL import Image


def lum(px):
    return (0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2]) / 255.0


def wedge_mean(img, cx, cy, r0, r1, angle_deg, half):
    total, count = 0.0, 0
    x0 = max(0, int(cx - r1) - 1)
    x1 = min(img.width, int(cx + r1) + 2)
    y0 = max(0, int(cy - r1) - 1)
    y1 = min(img.height, int(cy + r1) + 2)
    for y in range(y0, y1):
        for x in range(x0, x1):
            dx, dy = x - cx, y - cy
            d2 = dx * dx + dy * dy
            if d2 < r0 * r0 or d2 > r1 * r1:
                continue
            # clockwise from up: angle = atan2(dx, -dy)
            a = math.degrees(math.atan2(dx, -dy)) - angle_deg
            a = (a + 180.0) % 360.0 - 180.0
            if abs(a) <= half:
                total += lum(img.getpixel((x, y)))
                count += 1
    if count == 0:
        raise SystemExit(f"FAIL: empty wedge at {angle_deg} deg (r0={r0} r1={r1})")
    return total / count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--center", required=True)
    ap.add_argument("--radius", type=float, required=True)
    ap.add_argument("--shadow", type=float)
    ap.add_argument("--r0", type=float, default=0.45)
    ap.add_argument("--r1", type=float, default=0.60)
    ap.add_argument("--wedge", type=float, default=3.0)
    ap.add_argument("--expect", choices=["day", "night"])
    ap.add_argument("--max-center-lum", type=float)
    ap.add_argument("--pixel", action="append",
                    help="X,Y:RRGGBB, repeatable")
    ap.add_argument("--tol", type=int, default=24)
    args = ap.parse_args()

    failures = []

    img = Image.open(args.image).convert("RGB")
    cx, cy = (float(v) for v in args.center.split(","))

    if args.expect:
        if args.shadow is None:
            ap.error("--expect needs --shadow ANGLE_DEG")
        here = wedge_mean(img, cx, cy, args.r0 * args.radius,
                          args.r1 * args.radius, args.shadow, args.wedge)
        opp = wedge_mean(img, cx, cy, args.r0 * args.radius,
                         args.r1 * args.radius, args.shadow + 180.0, args.wedge)
        if args.expect == "day":
            if not here < 0.80 * opp:
                failures.append(
                    f"expect day: shadow wedge lum {here:.3f} not < 0.80 x opposite {opp:.3f}")
            else:
                print(f"ok day: shadow {here:.3f} < 0.80 x {opp:.3f}")
        else:  # night
            if abs(here - opp) > 0.15 * max(here, opp):
                failures.append(
                    f"expect night: wedges {here:.3f} vs {opp:.3f} differ > 15%")
            else:
                print(f"ok night: wedges {here:.3f} ~ {opp:.3f}")

    if args.max_center_lum is not None:
        vals = [lum(img.getpixel((int(cx) + dx, int(cy) + dy)))
                for dx in range(-5, 5) for dy in range(-5, 5)]
        mean = sum(vals) / len(vals)
        if mean > args.max_center_lum:
            failures.append(f"centre lum {mean:.1f} > {args.max_center_lum}")
        else:
            print(f"ok centre lum {mean:.1f} <= {args.max_center_lum}")

    for spec in args.pixel or []:
        xy, col = spec.split(":")
        px_x, px_y = (int(v) for v in xy.split(","))
        want = tuple(int(col[i:i + 2], 16) for i in (0, 2, 4))
        got = img.getpixel((px_x, px_y))
        if any(abs(g - w) > args.tol for g, w in zip(got, want)):
            failures.append(
                f"pixel {px_x},{px_y} = {got} not within {args.tol} of {want}")
        else:
            print(f"ok pixel {px_x},{px_y} = {got}")

    for f in failures:
        print("FAIL:", f)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
