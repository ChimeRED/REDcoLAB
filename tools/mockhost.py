# -*- coding: utf-8 -*-
"""
Mock CSP host for REDcoLAB filter plug-ins.

What this is
------------
A deliberately minimal stand-in for CLIP STUDIO PAINT that drives the plug-in
through its full lifecycle in-process, so the filter maths and the SDK calling
convention can be checked without launching CSP:

    moduleInitialize -> filterInitialize -> filterRun -> filterTerminate -> moduleTerminate

What this is NOT
----------------
It is NOT evidence that CSP loads the plug-in. The only evidence for that is a
real CSP run with the .cpm/.dll installed in a directory CSP actually scans.
Never report a mock PASS as "works in CSP".

Only the host callbacks the plug-in actually needs are registered. Everything
else is left NULL on purpose: a correct plug-in must survive a host that does
not expose optional procs (see grep REDCOLAB_LOG / error record ERR-20261015-001).

Usage:
    python tools/mockhost.py dist/REDcoLAB.cpm
    python tools/mockhost.py dist/REDcoLAB.cpm --expect-algorithm 1
Exit code 0 = PASS, 1 = FAIL.
"""

import argparse
import ctypes
import os
import sys

Int = ctypes.c_long          # TriglavPlugInInt is a 32-bit signed int on Windows
Bool = ctypes.c_ubyte        # TriglavPlugInBool
Ptr = ctypes.c_void_p

# ---- selectors / constants (mirrors sdk_official/TriglavPlugInDefine.h) ----
SEL_MODULE_INIT = 0x0101
SEL_MODULE_TERM = 0x0102
SEL_FILTER_INIT = 0x0201
SEL_FILTER_RUN = 0x0202
SEL_FILTER_TERM = 0x0203

CALL_OK = 0
API_OK = 0

STATE_START = 0x0101
STATE_CONTINUE = 0x0102
STATE_END = 0x0103
RES_CONTINUE = 0x0101
RES_EXIT = 0x0103

ORDER_RGBA = 0x03

MODULE_KIND_FILTER = 0x4380
TARGET_KINDS_EXPECTED = [0x102, 0x101, 0x106]

WIDTH, HEIGHT = 4, 2
ROW_BYTES = WIDTH * 4
PIXEL_BYTES = 4
BLOCK = None  # filled once Rect type exists

# The single test image both mockhost and algorithm_test describe.
# Distinct, non-gray colours chosen so every algorithm produces a different
# result, and with alpha values 255 / 128 / 0 / 1 to cover opaque, semi, fully
# transparent and near-transparent.
SOURCE = [
    (200, 40, 90, 255), (10, 220, 30, 128), (90, 90, 200, 0), (255, 0, 0, 255),
    (0, 255, 0, 200), (0, 0, 255, 1), (123, 45, 200, 255), (7, 8, 9, 64),
]


class Point(ctypes.Structure):
    _fields_ = [("x", Int), ("y", Int)]


class Rect(ctypes.Structure):
    _fields_ = [("left", Int), ("top", Int), ("right", Int), ("bottom", Int)]


BLOCK = Rect(0, 0, WIDTH, HEIGHT)


class Server(ctypes.Structure):
    """TriglavPlugInServer: recordSuite, serviceSuite, hostObject."""
    _fields_ = [("record", Ptr * 256), ("service", Ptr * 256), ("host", Ptr)]


def suite(n):
    """A flat array of function pointers, i.e. a service/record struct."""
    return (Ptr * n)()


def put(arr, index, func, keep):
    arr[index] = ctypes.cast(func, Ptr)
    keep.append(func)


def mk(*argtypes):
    """Build a ctypes CFUNCTYPE with an Int return, keeping a reference alive."""
    def deco(fn):
        cb = ctypes.CFUNCTYPE(Int, *argtypes)(fn)
        return cb
    return deco


def str_of(string_object):
    """Read a host string object.

    This mock never allocates real host strings: it hands the plug-in an opaque
    small integer handle and keeps the decoded text in STRING_TABLE. The plug-in
    must treat it as an opaque TriglavPlugInStringObject, which it does.
    """
    if not string_object:
        return None
    try:
        return STRING_TABLE.get(int(string_object), "<unknown handle>")
    except Exception:
        return "<unreadable>"


STRING_TABLE = {}


def _store(text):
    handle = len(STRING_TABLE) + 1
    STRING_TABLE[handle] = text
    return handle


def build(pixels, alpha, log, prop_state, minimal=False):
    """Create the service suite and record suite for one run. Returns (suite, keep).

    minimal=True strips every OPTIONAL host callback, leaving only the bare
    minimum the plug-in needs to do its job. A correct plug-in must survive a
    host that does not expose optional procs; that was a real crash before
    (see .agents/skill-error-record/plugin-dev-2026-10-15-null-host-proc.md),
    so it gets a permanent regression test.
    """
    keep = []
    svc = suite(256)

    # ---------------- string service (index 0) ----------------
    ss = suite(10)

    @mk(ctypes.POINTER(Ptr), ctypes.c_char_p, Int)
    def create_ascii(pout, text, length):
        raw = ctypes.string_at(text, length) if length else b""
        pout[0] = Ptr(_store(raw.decode("ascii", "replace")))
        return API_OK

    @mk(ctypes.POINTER(Ptr), ctypes.POINTER(ctypes.c_uint16), Int)
    def create_unicode(pout, text, length):
        raw = ctypes.string_at(ctypes.cast(text, Ptr), length * 2) if length else b""
        pout[0] = Ptr(_store(raw.decode("utf-16-le", "replace")))
        return API_OK

    @mk(Ptr)
    def release(obj):
        return API_OK

    put(ss, 0, create_ascii, keep)
    put(ss, 1, create_unicode, keep)
    put(ss, 5, release, keep)

    # ---------------- offscreen service (index 2) ----------------
    osvc = suite(20)

    @mk(ctypes.POINTER(Rect), Ptr)
    def get_rect(prect, obj):
        prect[0] = BLOCK
        return API_OK

    @mk(ctypes.POINTER(Int), Ptr)
    def get_channel_order(pv, obj):
        pv[0] = ORDER_RGBA
        return API_OK

    @mk(ctypes.POINTER(Int), ctypes.POINTER(Int), ctypes.POINTER(Int), Ptr)
    def get_rgb_index(pr, pg, pb, obj):
        pr[0], pg[0], pb[0] = 0, 1, 2
        return API_OK

    @mk(ctypes.POINTER(Int), Ptr, ctypes.POINTER(Rect))
    def get_block_count(pv, obj, bounds):
        pv[0] = 1
        return API_OK

    @mk(ctypes.POINTER(Rect), Int, Ptr, ctypes.POINTER(Rect))
    def get_block_rect(prect, index, obj, bounds):
        prect[0] = BLOCK
        return API_OK

    @mk(ctypes.POINTER(Ptr), ctypes.POINTER(Int), ctypes.POINTER(Int),
        ctypes.POINTER(Rect), Ptr, ctypes.POINTER(Point))
    def get_block_image(paddr, prow, ppix, prect, obj, ppos):
        paddr[0] = ctypes.cast(pixels, Ptr)
        prow[0] = ROW_BYTES
        ppix[0] = PIXEL_BYTES
        prect[0] = BLOCK
        return API_OK

    @mk(ctypes.POINTER(Ptr), ctypes.POINTER(Int), ctypes.POINTER(Int),
        ctypes.POINTER(Rect), Ptr, ctypes.POINTER(Point))
    def get_block_alpha(paddr, prow, ppix, prect, obj, ppos):
        paddr[0] = ctypes.cast(alpha, Ptr)
        prow[0] = WIDTH
        ppix[0] = 1
        prect[0] = BLOCK
        return API_OK

    put(osvc, 5, get_rect, keep)
    put(osvc, 7, get_channel_order, keep)
    put(osvc, 8, get_rgb_index, keep)
    put(osvc, 10, get_block_count, keep)
    put(osvc, 11, get_block_rect, keep)
    put(osvc, 12, get_block_image, keep)
    put(osvc, 13, get_block_alpha, keep)

    # ---------------- property service (index 4) ----------------
    ps = suite(20)

    @mk(ctypes.POINTER(Ptr))
    def prop_create(pout):
        pout[0] = Ptr(1)
        return API_OK

    @mk(Ptr)
    def prop_release(obj):
        return API_OK

    @mk(Ptr, Int, Int, Int, Int, Ptr, ctypes.c_char)
    def add_item(obj, key, vtype, vkind, ikind, caption, ak):
        log.append("addItem key=%d type=0x%X caption=%r" % (key, vtype, str_of(caption)))
        prop_state.setdefault(key, {})
        return API_OK

    @mk(Ptr, Int, Int)
    def set_int(obj, key, value):
        prop_state.setdefault(key, {})["value"] = value
        return API_OK

    @mk(ctypes.POINTER(Int), Ptr, Int)
    def get_int(pvalue, obj, key):
        pvalue[0] = prop_state.get(key, {}).get("value", 0)
        return API_OK

    @mk(Ptr, Int, Int)
    def set_int_default(obj, key, value):
        prop_state.setdefault(key, {})["default"] = value
        return API_OK

    @mk(Ptr, Int, Int)
    def set_int_min(obj, key, value):
        prop_state.setdefault(key, {})["min"] = value
        return API_OK

    @mk(Ptr, Int, Int)
    def set_int_max(obj, key, value):
        prop_state.setdefault(key, {})["max"] = value
        return API_OK

    put(ps, 0, prop_create, keep)
    put(ps, 2, prop_release, keep)
    put(ps, 3, add_item, keep)
    put(ps, 8, set_int, keep)
    put(ps, 9, get_int, keep)
    put(ps, 10, set_int_default, keep)
    put(ps, 12, set_int_min, keep)
    put(ps, 14, set_int_max, keep)

    # ---------------- property service 2 (index 9) ----------------
    ps2 = suite(24)

    @mk(Ptr, Int, Bool)
    def set_store(obj, key, value):
        prop_state.setdefault(key, {})["store"] = value
        return API_OK

    @mk(Ptr, Int, Int)
    def set_enum(obj, key, value):
        prop_state.setdefault(key, {})["value"] = value
        return API_OK

    @mk(ctypes.POINTER(Int), Ptr, Int)
    def get_enum(pvalue, obj, key):
        pvalue[0] = prop_state.get(key, {}).get("value", 0)
        return API_OK

    @mk(Ptr, Int, Int)
    def set_enum_default(obj, key, value):
        prop_state.setdefault(key, {})["default"] = value
        return API_OK

    @mk(Ptr, Int, Int, Ptr, ctypes.c_char)
    def add_enum_item(obj, key, value, caption, ak):
        log.append("enumItem key=%d value=%d caption=%r" % (key, value, str_of(caption)))
        prop_state.setdefault(key, {}).setdefault("items", []).append(value)
        return API_OK

    put(ps2, 0, set_store, keep)
    put(ps2, 13, set_enum, keep)
    put(ps2, 14, get_enum, keep)
    put(ps2, 15, set_enum_default, keep)
    put(ps2, 17, add_enum_item, keep)

    svc[0] = ctypes.cast(ss, Ptr)      # stringService
    svc[2] = ctypes.cast(osvc, Ptr)    # offscreenService
    svc[4] = ctypes.cast(ps, Ptr)      # propertyService
    svc[9] = ctypes.cast(ps2, Ptr)     # propertyService2

    # ---------------- record suite ----------------
    rec = suite(256)

    # moduleInitializeRecord (index 0)
    mi = suite(3)

    @mk(ctypes.POINTER(Int), Ptr)
    def get_host_version(pversion, host):
        pversion[0] = 1
        return API_OK

    @mk(Ptr, Ptr)
    def set_module_id(host, string_object):
        log.append("moduleID = %s" % str_of(string_object))
        return API_OK

    @mk(Ptr, Int)
    def set_module_kind(host, kind):
        log.append("moduleKind = 0x%X" % kind)
        return API_OK

    put(mi, 0, get_host_version, keep)
    put(mi, 1, set_module_id, keep)
    put(mi, 2, set_module_kind, keep)

    # filterInitializeRecord (index 5)
    fi = suite(7)

    @mk(Ptr, Ptr, ctypes.c_char)
    def set_category(host, string_object, ak):
        log.append("category = %r" % str_of(string_object))
        return API_OK

    @mk(Ptr, Ptr, ctypes.c_char)
    def set_filter_name(host, string_object, ak):
        log.append("filterName = %r" % str_of(string_object))
        return API_OK

    @mk(Ptr, Bool)
    def set_can_preview(host, value):
        log.append("canPreview = %d" % value)
        return API_OK

    @mk(Ptr, Bool)
    def set_use_blank(host, value):
        return API_OK

    @mk(Ptr, ctypes.POINTER(Int), Int)
    def set_target_kinds(host, arr, count):
        vals = [arr[i] for i in range(count)]
        log.append("targetKinds = %s" % ["0x%X" % v for v in vals])
        prop_state["__targets__"] = vals
        return API_OK

    @mk(Ptr, Ptr)
    def set_property(host, obj):
        log.append("property set")
        return API_OK

    @mk(Ptr, Ptr, Ptr)
    def set_property_callback(host, callback, data):
        log.append("propertyCallBack set")
        return API_OK

    put(fi, 0, set_category, keep)
    put(fi, 1, set_filter_name, keep)
    put(fi, 2, set_can_preview, keep)
    put(fi, 3, set_use_blank, keep)
    put(fi, 4, set_target_kinds, keep)
    put(fi, 5, set_property, keep)
    put(fi, 6, set_property_callback, keep)

    # filterRunRecord (index 6)
    fr = suite(19)

    @mk(ctypes.POINTER(Ptr), Ptr)
    def get_property(pobj, host):
        pobj[0] = Ptr(1)
        return API_OK

    @mk(ctypes.POINTER(Ptr), Ptr)
    def get_destination(pobj, host):
        pobj[0] = Ptr(1)
        return API_OK

    @mk(ctypes.POINTER(Rect), Ptr)
    def get_select_rect(prect, host):
        prect[0] = BLOCK
        return API_OK

    @mk(ctypes.POINTER(Bool), Ptr)
    def has_select_area(pvalue, host):
        pvalue[0] = 0
        return API_OK

    @mk(Ptr, ctypes.POINTER(Rect))
    def update_destination(host, prect):
        return API_OK

    @mk(ctypes.POINTER(Int), Ptr, Int)
    def process(presult, host, state):
        presult[0] = RES_CONTINUE if state != STATE_END else RES_EXIT
        return API_OK

    @mk(Ptr, Int)
    def progress_total(host, value):
        return API_OK

    @mk(Ptr, Int)
    def progress_done(host, value):
        return API_OK

    put(fr, 0, get_property, keep)
    put(fr, 8, get_destination, keep)
    put(fr, 9, get_select_rect, keep)
    put(fr, 10, has_select_area, keep)
    put(fr, 12, update_destination, keep)
    put(fr, 16, process, keep)
    put(fr, 17, progress_total, keep)
    put(fr, 18, progress_done, keep)

    if minimal:
        # Deliberately remove only OPTIONAL procs the plug-in must be able to
        # live without. getHostVersion stays: it is a required callback (the
        # plug-in must know the host version before it can proceed), so a host
        # without it is not a valid host and refusing to load is correct.
        fr[9] = Ptr(None)    # getSelectAreaRectProc
        fr[10] = Ptr(None)   # hasSelectAreaOffscreenProc
        fr[12] = Ptr(None)   # updateDestinationOffscreenRectProc
        fr[17] = Ptr(None)   # setProgressTotalProc
        fr[18] = Ptr(None)   # setProgressDoneProc
        fi[3] = Ptr(None)    # setUseBlankImageProc
        ps[2] = Ptr(None)    # property releaseProc

    rec[0] = ctypes.cast(mi, Ptr)
    rec[5] = ctypes.cast(fi, Ptr)
    rec[6] = ctypes.cast(fr, Ptr)

    return svc, rec, keep


def main():
    parser = argparse.ArgumentParser(description="Mock CSP host for REDcoLAB filter plug-ins")
    parser.add_argument("plugin", help="path to REDcoLAB.cpm / REDcoLAB.dll")
    parser.add_argument("--expect-algorithm", type=int, default=None,
                        help="select a different algorithm before filterRun and expect it applied")
    parser.add_argument("--quiet", action="store_true", help="only print the verdict")
    parser.add_argument("--minimal", action="store_true",
                        help="strip optional host callbacks to test plug-in robustness")
    args = parser.parse_args()

    if not os.path.isfile(args.plugin):
        print("FAIL: plug-in not found: %s" % args.plugin)
        return 1

    pixels = (ctypes.c_ubyte * (WIDTH * HEIGHT * 4))()
    alpha = (ctypes.c_ubyte * (WIDTH * HEIGHT))()
    for i, (r, g, b, a) in enumerate(SOURCE):
        pixels[i * 4 + 0] = r
        pixels[i * 4 + 1] = g
        pixels[i * 4 + 2] = b
        pixels[i * 4 + 3] = a
        alpha[i] = a

    alpha_before = list(alpha)
    log = []
    prop_state = {}
    svc, rec, keep = build(pixels, alpha, log, prop_state, minimal=args.minimal)

    server = Server()
    ctypes.memmove(ctypes.byref(server, Server.record.offset), rec, ctypes.sizeof(rec))
    ctypes.memmove(ctypes.byref(server, Server.service.offset), svc, ctypes.sizeof(svc))
    server.host = Ptr(1)

    lib = ctypes.CDLL(os.path.abspath(args.plugin))
    if not hasattr(lib, "TriglavPluginCall"):
        print("FAIL: plug-in does not export TriglavPluginCall")
        return 1

    entry = lib.TriglavPluginCall
    entry.restype = None
    entry.argtypes = [ctypes.POINTER(Int), ctypes.POINTER(Ptr), Int,
                      ctypes.POINTER(Server), Ptr]

    result = Int(-1)
    data = Ptr(None)

    def call(selector, name):
        result.value = -1
        entry(ctypes.byref(result), ctypes.byref(data), selector, ctypes.byref(server), Ptr(None))
        ok = result.value == CALL_OK
        if not args.quiet:
            print("[%-15s] %s" % (name, "OK" if ok else "FAILED(%d)" % result.value))
        return ok

    ok = True
    ok &= call(SEL_MODULE_INIT, "moduleInitialize")
    ok &= call(SEL_FILTER_INIT, "filterInitialize")

    # The user changes parameters *after* the dialog opens, so the value has to
    # be written between filterInitialize and filterRun to mirror real CSP
    # timing. Writing it earlier would be overwritten by the plug-in's own
    # registration defaults, which would silently make this flag do nothing.
    if args.expect_algorithm is not None:
        prop_state.setdefault(1, {})["value"] = args.expect_algorithm

    ok &= call(SEL_FILTER_RUN, "filterRun")
    ok &= call(SEL_FILTER_TERM, "filterTerminate")
    ok &= call(SEL_MODULE_TERM, "moduleTerminate")

    after = bytes(pixels)
    alpha_after = list(alpha)

    problems = []

    # 1. every written pixel must be gray, EXCEPT fully transparent pixels:
    #    the official sample skips alpha == 0, so "untouched" is correct there
    #    and is asserted separately below.
    for i in range(WIDTH * HEIGHT):
        r, g, b, a = after[i * 4:i * 4 + 4]
        if SOURCE[i][3] == 0:
            continue
        if not (r == g == b):
            problems.append("pixel %d is not gray: rgb=(%d,%d,%d)" % (i, r, g, b))

    # 2. alpha must not change
    if alpha_before != alpha_after:
        problems.append("alpha changed: %s -> %s" % (alpha_before, alpha_after))

    # 3. pixels with alpha 0 must be left alone
    for i in range(WIDTH * HEIGHT):
        if SOURCE[i][3] == 0:
            got = tuple(after[i * 4:i * 4 + 4])
            if got != SOURCE[i]:
                problems.append("fully transparent pixel %d was modified: %s -> %s"
                                % (i, SOURCE[i], got))

    # 4. registration must report the filter kind and the raster targets
    kind_line = [l for l in log if l.startswith("moduleKind")]
    if not kind_line or ("0x%X" % MODULE_KIND_FILTER) not in kind_line[0]:
        problems.append("moduleKind was not reported as Filter (0x%X): %s"
                        % (MODULE_KIND_FILTER, kind_line))

    if not args.minimal:
        reported_targets = prop_state.get("__targets__", [])
        if reported_targets != TARGET_KINDS_EXPECTED:
            problems.append("targetKinds mismatch: %s != %s"
                            % (["0x%X" % v for v in reported_targets],
                               ["0x%X" % v for v in TARGET_KINDS_EXPECTED]))

    if not args.quiet:
        print("")
        print("before:", [tuple(SOURCE[i]) for i in range(WIDTH * HEIGHT)])
        print("after :", [tuple(after[i * 4:i * 4 + 4]) for i in range(WIDTH * HEIGHT)])
        print("")
        print("--- host log ---")
        for line in log:
            print("  " + line)

    if problems:
        print("")
        for p in problems:
            print("PROBLEM: " + p)
        print("")
        print("RESULT: FAIL")
        return 1

    print("")
    print("checks: all pixels R==G==B, alpha unchanged, transparent pixels untouched, targets correct")
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
