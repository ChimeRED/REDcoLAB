# -*- coding: utf-8 -*-
"""
Exhaustive grayscale algorithm / strength matrix test for REDcoLAB.

mockhost.py proves the lifecycle works. This proves the *maths* is right for
every algorithm and every strength boundary, by comparing the plug-in's output
against an independent reimplementation of the formulas.

The reference implementation here is deliberately written separately from
src/grayscale.h: if both were derived from the same code they could share the
same mistake. These are the textbook formulas.

Usage:
    python tools/algorithm_test.py dist/REDcoLAB.cpm
Exit code 0 = all assertions pass, 1 = mismatch.
"""

import ctypes
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import mockhost as mh
from mockhost import Int, Ptr, Server, CALL_OK
from mockhost import (SEL_MODULE_INIT, SEL_FILTER_INIT, SEL_FILTER_RUN,
                      SEL_FILTER_TERM, SEL_MODULE_TERM, WIDTH, HEIGHT)

# ---- independent reference implementation (textbook formulas) ----
ALGORITHM_NAMES = [
    "Rec.709 weighted", "Rec.601 weighted", "average", "max", "min",
    "red channel", "green channel", "blue channel",
]


def reference_gray(r, g, b, algorithm):
    if algorithm == 0:
        return (2126 * r + 7152 * g + 722 * b + 5000) // 10000
    if algorithm == 1:
        return (2990 * r + 5870 * g + 1140 * b + 5000) // 10000
    if algorithm == 2:
        return (r + g + b + 1) // 3
    if algorithm == 3:
        return max(r, g, b)
    if algorithm == 4:
        return min(r, g, b)
    if algorithm == 5:
        return r
    if algorithm == 6:
        return g
    if algorithm == 7:
        return b
    raise ValueError("unknown algorithm %d" % algorithm)


def reference_blend(channel, gray, strength):
    """Mirror of redcolab::LerpU8 - rounding half away from zero."""
    if strength <= 0:
        return channel
    v = (channel * 100 + (gray - channel) * strength + 50) // 100
    return max(0, min(255, v))


# Use the exact same source pixels as mockhost so both tests describe one image.
SOURCE = mh.SOURCE


def run_once(plugin, algorithm, strength):
    """Drive the plug-in once and return (pixels, alpha) after processing."""
    pixels = (ctypes.c_ubyte * (WIDTH * HEIGHT * 4))()
    alpha = (ctypes.c_ubyte * (WIDTH * HEIGHT))()
    for i, (r, g, b, a) in enumerate(SOURCE):
        pixels[i * 4 + 0] = r
        pixels[i * 4 + 1] = g
        pixels[i * 4 + 2] = b
        pixels[i * 4 + 3] = a
        alpha[i] = a

    log = []
    prop_state = {}
    svc, rec, keep = mh.build(pixels, alpha, log, prop_state)

    server = Server()
    ctypes.memmove(ctypes.byref(server, Server.record.offset), rec, ctypes.sizeof(rec))
    ctypes.memmove(ctypes.byref(server, Server.service.offset), svc, ctypes.sizeof(svc))
    server.host = Ptr(1)

    lib = ctypes.CDLL(os.path.abspath(plugin))
    entry = lib.TriglavPluginCall
    entry.restype = None
    entry.argtypes = [ctypes.POINTER(Int), ctypes.POINTER(Ptr), Int,
                      ctypes.POINTER(Server), Ptr]

    result = Int(-1)
    data = Ptr(None)

    def call(selector):
        result.value = -1
        entry(ctypes.byref(result), ctypes.byref(data), selector, ctypes.byref(server), Ptr(None))
        return result.value == CALL_OK

    ok = call(SEL_MODULE_INIT) and call(SEL_FILTER_INIT)
    # parameters are changed after the dialog opens, i.e. before the run
    prop_state.setdefault(1, {})["value"] = algorithm
    prop_state.setdefault(2, {})["value"] = strength
    ok = call(SEL_FILTER_RUN) and ok
    call(SEL_FILTER_TERM)
    call(SEL_MODULE_TERM)

    if not ok:
        raise RuntimeError("plug-in call chain failed for algorithm=%d strength=%d"
                           % (algorithm, strength))
    return list(bytes(pixels)), list(alpha)


def main():
    plugin = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "dist", "REDcoLAB.cpm")

    if not os.path.isfile(plugin):
        print("FAIL: plug-in not found: %s" % plugin)
        return 1

    print("algorithm / strength matrix test")
    print("plug-in: %s" % os.path.basename(plugin))
    print("")

    failures = []

    # ---- 1. every algorithm at full strength ----
    print("1) all 8 algorithms at strength=100, compared against reference formulas")
    for algorithm in range(8):
        pixels, alpha = run_once(plugin, algorithm, 100)
        for i, (r, g, b, a) in enumerate(SOURCE):
            got = tuple(pixels[i * 4:i * 4 + 4])
            if a == 0:
                # fully transparent pixels are skipped by design
                expected = (r, g, b, a)
            else:
                gray = reference_gray(r, g, b, algorithm)
                expected = (gray, gray, gray, a)
            if got != expected:
                failures.append("alg=%d(%s) pixel %d: got %s expected %s"
                                % (algorithm, ALGORITHM_NAMES[algorithm], i, got, expected))
        print("   [%d] %-16s %s" % (algorithm, ALGORITHM_NAMES[algorithm],
                                    "OK" if not failures else "see failures"))

    # ---- 2. strength sweep on one representative pixel ----
    print("")
    print("2) strength sweep 0..100 step 10 on pixel 0 (200,40,90), Rec.709")
    for strength in range(0, 101, 10):
        pixels, alpha = run_once(plugin, 0, strength)
        got = tuple(pixels[0:4])
        gray = reference_gray(200, 40, 90, 0)
        expected = (reference_blend(200, gray, strength),
                    reference_blend(40, gray, strength),
                    reference_blend(90, gray, strength),
                    255)
        status = "OK" if got == expected else "MISMATCH"
        if got != expected:
            failures.append("strength=%d: got %s expected %s" % (strength, got, expected))
        print("   s=%3d -> %-22s expected %-22s %s" % (strength, got, expected, status))

    # ---- 3. alpha invariance and transparent-pixel safety across the matrix ----
    print("")
    print("3) alpha invariance + transparent-pixel safety across algorithms x strengths")
    combos = 0
    for algorithm in range(8):
        for strength in (0, 50, 100):
            pixels, alpha = run_once(plugin, algorithm, strength)
            combos += 1
            expected_alpha = [px[3] for px in SOURCE]
            if alpha != expected_alpha:
                failures.append("alpha changed: alg=%d s=%d %s != %s"
                                % (algorithm, strength, alpha, expected_alpha))
            transparent = tuple(pixels[2 * 4:2 * 4 + 4])
            if transparent != SOURCE[2]:
                failures.append("transparent pixel modified: alg=%d s=%d -> %s"
                                % (algorithm, strength, transparent))
    print("   %d combinations checked" % combos)

    # ---- verdict ----
    print("")
    if failures:
        for f in failures:
            print("MISMATCH: " + f)
        print("")
        print("RESULT: FAIL (%d problem(s))" % len(failures))
        return 1

    print("RESULT: PASS (8 algorithms x full strength, strength sweep, alpha invariance)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
