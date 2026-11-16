#!/usr/bin/env bash
set -euo pipefail

VERSION=0.15.1b
TARBALL="libmad-${VERSION}.tar.gz"
URL="https://downloads.sourceforge.net/mad/${TARBALL}"
MD5="1be543bc30c56fb6bea1d7bf6a64e66c"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${ROOT}/libs/build"
PREFIX="${ROOT}/libs/libmad"
CC="${CC:-arm-none-eabi-gcc}"
AR="${AR:-arm-none-eabi-ar}"
RANLIB="${RANLIB:-arm-none-eabi-ranlib}"

command -v "$CC" >/dev/null || { echo "arm-none-eabi-gcc not found in PATH" >&2; exit 1; }
mkdir -p "$WORK"
cd "$WORK"
if [[ ! -f "$TARBALL" ]]; then curl -fL --retry 3 "$URL" -o "$TARBALL"; fi
echo "${MD5}  ${TARBALL}" | md5sum -c -
rm -rf "libmad-${VERSION}"
tar xzf "$TARBALL"
cd "libmad-${VERSION}"
# The upstream release archive carries an x86-selected public header.
sed -i 's/^# define FPM_INTEL$/# define FPM_ARM/' mad.h
grep -q '^# define FPM_ARM$' mad.h || { echo "Could not set libmad FPM_ARM" >&2; exit 1; }
CFLAGS="-O2 -marm -march=armv5te -mtune=arm946e-s -ffunction-sections -fdata-sections -fomit-frame-pointer -DFPM_ARM -DOPT_SPEED -DOPT_SSO -DASO_IMDCT -DASO_INTERLEAVE1 -DNDEBUG -DHAVE_ASSERT_H -DSIZEOF_INT=4 -DSIZEOF_LONG=4 -DSIZEOF_LONG_LONG=8 -std=gnu89 -w"
SOURCES="version.c fixed.c bit.c timer.c stream.c frame.c synth.c decoder.c layer12.c layer3.c huffman.c"
for src in $SOURCES; do "$CC" $CFLAGS -c "$src" -o "${src%.c}.o"; done
"$CC" $CFLAGS -c imdct_l_arm.S -o imdct_l_arm.o
rm -f libmad.a
"$AR" rcs libmad.a version.o fixed.o bit.o timer.o stream.o frame.o synth.o decoder.o layer12.o layer3.o huffman.o imdct_l_arm.o
"$RANLIB" libmad.a
mkdir -p "${PREFIX}/include" "${PREFIX}/lib"
cp mad.h "${PREFIX}/include/mad.h"
cp libmad.a "${PREFIX}/lib/libmad.a"
cp COPYING "${PREFIX}/COPYING" 2>/dev/null || true
echo "libmad ${VERSION} built for ARM9 FPM_ARM"
