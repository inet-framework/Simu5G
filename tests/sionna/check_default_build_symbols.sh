#!/usr/bin/env bash
#
#                  Simu5G
#
# Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
# Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
#
# This file is part of a software released under the license included in file
# "license.pdf". Please read LICENSE and README files before using it.
# The above files and the present reference are part of the software itself,
# and cannot be removed from it.
#
#
# SEAM-02 gate: prove the DEFAULT (Simu5G_Sionna feature OFF) build links zero
# Sionna / HDF5 / Python / TensorFlow / Torch symbols. The default deep build must
# stay byte-for-byte unaffected by the opt-in Sionna feature; if any of these
# symbols appear in the default binary, the build-isolation seam has leaked and the
# gate fails (nonzero exit).
#
# Usage:
#   tests/sionna/check_default_build_symbols.sh [binary-or-library-path]
#
# If no path is given, the script probes the usual Simu5G output locations.

set -euo pipefail

# Symbols that must NEVER appear in a default build.
#
# Two precise pattern classes, matched against DEMANGLED symbol names (see below):
#
#  - The Sionna integration C++ lives in namespace/classes named `Sionna*`
#    (SionnaChannelModel, SionnaManager, SionnaTable) under the sionna/ source
#    folder. We anchor the Sionna match to the demangled `Sionna` identifier with a
#    leading non-identifier boundary so it cannot false-positive on unrelated
#    Simu5G symbols that merely contain the substring "sionna" (e.g.
#    `RtVideoStreamingSender::handleStartSessionNack`, which contains "ssionNa").
#  - The offline-stack dependency symbols (hdf5/python/tensorflow/torch) would only
#    ever appear as substrings of vendored library symbols, so those stay as plain
#    case-insensitive substrings — the full detection set is unweakened.
#
# Canonical forbidden set (applied below as two precise passes against DEMANGLED
# names): Sionna (identifier-boundary, case-sensitive) + hdf5|python|tensorflow|torch
# (substring, case-insensitive).

# Resolve the repository root from this script's location so the probe works
# regardless of the caller's current working directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Build the list of candidate binaries/libraries to inspect.
#
# NOTE on `bin/simu5g`: on Linux that path is an opp_run *launcher shell script*,
# not an inspectable ELF object — `nm` yields nothing from it. The real inspectable
# artifacts are the `libsimu5g*.so` shared libraries opp_makemake produces under
# src/ (and out/<mode>/src/). So the shared libraries are probed FIRST; the bin/
# wrapper scripts are kept only as a last-ditch fallback (e.g. platforms where the
# linked executable, not a wrapper, lands at bin/). A candidate that `nm` cannot
# read is skipped rather than treated as the target, so a launcher script never
# masks a readable library behind it.
candidates=()
if [[ $# -ge 1 && -n "${1:-}" ]]; then
    candidates+=("$1")
else
    # Shared/static libraries produced by opp_makemake (path varies by mode/arch) —
    # these are the genuine ELF/Mach-O artifacts and are probed first.
    while IFS= read -r lib; do
        candidates+=("${lib}")
    done < <(find "${REPO_ROOT}/src" "${REPO_ROOT}/out" -maxdepth 6 \
                 \( -name 'libsimu5g*.so' -o -name 'libsimu5g*.a' -o -name 'libsimu5g*.dylib' \) \
                 2>/dev/null || true)
    # Fallback: a real linked executable at bin/ (skipped automatically if it is a
    # launcher shell script, via the nm-readability check below).
    candidates+=(
        "${REPO_ROOT}/bin/simu5g"
        "${REPO_ROOT}/bin/simu5g_dbg"
    )
fi

# Returns 0 if `nm` can read at least one symbol from the given file.
# Capture nm output into a variable instead of piping into `grep -q` — under
# `set -o pipefail`, `grep -q` closing the pipe early sends SIGPIPE to nm and makes
# the whole pipeline report failure even for a perfectly readable object.
nm_readable() {
    local f="$1" out
    [[ -e "${f}" ]] || return 1
    out="$(nm -DC "${f}" 2>/dev/null || true)"
    [[ -n "${out}" ]] && return 0
    out="$(nm -C "${f}" 2>/dev/null || true)"
    [[ -n "${out}" ]] && return 0
    return 1
}

# Pick the first candidate that exists AND is nm-readable (skip launcher scripts).
target=""
for c in "${candidates[@]}"; do
    if nm_readable "${c}"; then
        target="${c}"
        break
    fi
done

if [[ -z "${target}" ]]; then
    echo "ERROR: no inspectable default Simu5G library/binary found." >&2
    echo "       (bin/simu5g is a launcher script on Linux, not an object file.)" >&2
    echo "       Build the default target first (e.g. 'make' with Simu5G_Sionna OFF)," >&2
    echo "       or pass an explicit path: $0 <binary-or-library>" >&2
    exit 2
fi

echo "SEAM-02 symbol check: inspecting ${target}"

# Extract symbol names. Prefer dynamic symbols (-D); fall back to the full table
# (static archives / stripped-dynamic objects yield nothing from 'nm -D').
# DEMANGLE the names (nm -C / c++filt) so the precise identifier-boundary match on
# `Sionna` works on readable C++ names (e.g. `simu5g::SionnaManager::...`) rather
# than mangled forms — and so it cannot false-positive on substrings buried in
# unrelated mangled symbols.
symbols="$(nm -DC "${target}" 2>/dev/null || true)"
if [[ -z "${symbols}" ]]; then
    symbols="$(nm -C "${target}" 2>/dev/null || true)"
fi
# Last-resort fallback if this nm lacks -C: demangle via c++filt if available.
if [[ -z "${symbols}" ]]; then
    raw="$(nm -D "${target}" 2>/dev/null || nm "${target}" 2>/dev/null || true)"
    if command -v c++filt >/dev/null 2>&1; then
        symbols="$(printf '%s\n' "${raw}" | c++filt || true)"
    else
        symbols="${raw}"
    fi
fi

if [[ -z "${symbols}" ]]; then
    echo "ERROR: could not read any symbols from ${target} (nm produced no output)." >&2
    exit 2
fi

# Match in two precise passes:
#  - Sionna integration code: CASE-SENSITIVE identifier-boundary match on `Sionna`
#    (the class/namespace convention) — avoids the `SessionNack`-style false positive.
#  - Offline dependency libraries: case-insensitive substring match.
sionna_matches="$(printf '%s\n' "${symbols}" | grep -E '(^|[^A-Za-z0-9_])Sionna' || true)"
dep_matches="$(printf '%s\n' "${symbols}" | grep -iE 'hdf5|python|tensorflow|torch' || true)"
matches="$(printf '%s\n%s\n' "${sionna_matches}" "${dep_matches}" | grep -v '^$' || true)"

if [[ -n "${matches}" ]]; then
    echo "FAIL: forbidden symbols found in the default build (${target}):" >&2
    printf '%s\n' "${matches}" >&2
    echo "SEAM-02 violated: the default build is NOT free of Sionna/HDF5/Python symbols." >&2
    exit 1
fi

echo "PASS: ${target} contains no sionna/hdf5/python/tensorflow/torch symbols (SEAM-02 OK)."
exit 0
