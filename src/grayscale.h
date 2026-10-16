/*
 * REDcoLAB - grayscale algorithms
 *
 * Pure functions only: no SDK types, no I/O, no state. That keeps the maths
 * independently reviewable and lets tools/verify_grayscale.py mirror it.
 *
 * Copyright (c) REDcoLAB contributors. Plugin code only; no CELSYS code here.
 */
#ifndef REDCOLAB_GRAYSCALE_H
#define REDCOLAB_GRAYSCALE_H

namespace redcolab {

/* Property item keys reported to the host and read back at run time. */
enum ItemKey {
    kItemAlgorithm = 1,
    kItemStrength  = 2,
    kItemInvert    = 3
};

/* Enumeration values for kItemAlgorithm, in menu order. */
enum Algorithm {
    kAlgoRec709  = 0,  /* ITU-R BT.709 luma: 0.2126 R + 0.7152 G + 0.0722 B */
    kAlgoRec601  = 1,  /* ITU-R BT.601 luma: 0.299 R + 0.587 G + 0.114 B */
    kAlgoAverage = 2,  /* (R + G + B) / 3 */
    kAlgoMax     = 3,  /* max(R, G, B) - keeps highlights */
    kAlgoMin     = 4,  /* min(R, G, B) - keeps shadows */
    kAlgoRed     = 5,
    kAlgoGreen   = 6,
    kAlgoBlue    = 7,
    kAlgoCount   = 8
};

/* Linear interpolation, rounding half away from zero.
 * tMax is passed in so the numerator is always non-negative. */
inline int LerpU8(int from, int to, int t, int tMax) {
    if (tMax <= 0) { return from; }
    int v = (from * tMax + (to - from) * t + tMax / 2) / tMax;
    if (v < 0) { v = 0; }
    if (v > 255) { v = 255; }
    return v;
}

/* Weighted luma using integer weights scaled by 10000, rounded to nearest. */
inline int WeightedLuma(int r, int g, int b, int wr, int wg, int wb) {
    return (wr * r + wg * g + wb * b + 5000) / 10000;
}

inline int GrayFromRGB(int r, int g, int b, int algorithm) {
    switch (algorithm) {
        case kAlgoRec601:  return WeightedLuma(r, g, b, 2990, 5870, 1140);
        case kAlgoAverage: return (r + g + b + 1) / 3;
        case kAlgoMax:     return (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
        case kAlgoMin:     return (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
        case kAlgoRed:     return r;
        case kAlgoGreen:   return g;
        case kAlgoBlue:    return b;
        case kAlgoRec709:
        default:           return WeightedLuma(r, g, b, 2126, 7152, 722);
    }
}

/* Convert one pixel in place: RGB channels only, alpha untouched.
 * strength 0 = unchanged, 100 = fully gray. invert flips the gray value.
 * Returns the resulting gray value (before strength blending). */
inline int ApplyPixel(unsigned char* rgb, int algorithm, int strength, bool invert) {
    const int r = rgb[0];
    const int g = rgb[1];
    const int b = rgb[2];

    int gray = GrayFromRGB(r, g, b, algorithm);
    if (invert) { gray = 255 - gray; }
    if (gray < 0) { gray = 0; }
    if (gray > 255) { gray = 255; }

    rgb[0] = static_cast<unsigned char>(LerpU8(r, gray, strength, 100));
    rgb[1] = static_cast<unsigned char>(LerpU8(g, gray, strength, 100));
    rgb[2] = static_cast<unsigned char>(LerpU8(b, gray, strength, 100));
    return gray;
}

} /* namespace redcolab */

#endif /* REDCOLAB_GRAYSCALE_H */
