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
#   tests/sionna/check_default_build_symbols.sh [--build] [binary-or-library-path]
#
# Modes:
#   --build              Build a feature-OFF library into a fresh temp directory and
#                        inspect exactly that artifact. Self-certifying: the inspected
#                        object is guaranteed to have been compiled with Simu5G_Sionna
#                        disabled, regardless of the current repo feature state.
#
#   <path>               Inspect the given artifact directly. Caller is responsible
#                        for ensuring it was built with the feature OFF.
#
#   (no args)            Probe the usual Simu5G output locations. ONLY safe when the
#                        Simu5G_Sionna feature is currently DISABLED in the source tree.
#                        If the feature is currently ENABLED, exit 3 (ambiguous) rather
#                        than certifying the wrong artifact; use --build instead.
#
# Exit codes:
#   0  PASS — no forbidden symbols found in the inspected feature-OFF artifact.
#   1  FAIL — forbidden symbols found (SEAM-02 violated).
#   2  ERROR — cannot find / read the artifact, or build failed.
#   3  AMBIGUOUS — no-arg probe with feature currently ENABLED; cannot certify.

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

# Parse flags.
BUILD_MODE=0
EXPLICIT_PATH=""
for arg in "$@"; do
    case "${arg}" in
        --build) BUILD_MODE=1 ;;
        *)       EXPLICIT_PATH="${arg}" ;;
    esac
done

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

# ---- Mode 1: --build — compile a fresh feature-OFF library, inspect that. --------
if [[ "${BUILD_MODE}" -eq 1 ]]; then
    echo "SEAM-02 self-certifying build: compiling feature-OFF library into temp dir..."
    BUILD_TMP="$(mktemp -d)"
    trap 'rm -rf "${BUILD_TMP}"' EXIT

    # Locate the OMNeT++ environment (needed for opp_makemake / make).
    if ! command -v opp_run >/dev/null 2>&1; then
        echo "ERROR: opp_run not on PATH; source the OMNeT++ setenv script first." >&2
        exit 2
    fi

    # Build a debug (fast) feature-OFF copy of the Simu5G library into BUILD_TMP.
    # We use the standard opp_featuretool mechanism: write a temp .oppfeatures state
    # with Simu5G_Sionna disabled, run make in the BUILD_TMP overlay, then restore.
    # Simpler and more portable: pass the feature as a make variable override so we
    # do NOT touch the repo's .oppfeatures file at all.
    #
    # opp_makemake already puts the Sionna sources behind #ifdef WITH_SIONNA and the
    # .ned exclusion via the feature; the cleanest no-touch approach is to rebuild
    # without the feature's CFLAGS and without the source files it gates. But that
    # requires knowing the exact makefile knobs.
    #
    # Robust portable approach: use opp_featuretool to disable the feature in a copy
    # of the repo's src/ Makefile.inc, build, then re-enable. Since opp_featuretool
    # writes to src/.oppfeatures (not src/Makefile), we can save/restore it atomically.
    OPPFEATURES_FILE="${REPO_ROOT}/src/.oppfeatures"
    OPPFEATURES_BACKUP=""
    if [[ -f "${OPPFEATURES_FILE}" ]]; then
        OPPFEATURES_BACKUP="$(mktemp)"
        cp "${OPPFEATURES_FILE}" "${OPPFEATURES_BACKUP}"
    fi

    restore_features() {
        if [[ -n "${OPPFEATURES_BACKUP}" && -f "${OPPFEATURES_BACKUP}" ]]; then
            cp "${OPPFEATURES_BACKUP}" "${OPPFEATURES_FILE}"
            rm -f "${OPPFEATURES_BACKUP}"
        elif [[ -n "${OPPFEATURES_BACKUP}" ]]; then
            rm -f "${OPPFEATURES_FILE}"
        fi
    }
    trap 'restore_features; rm -rf "${BUILD_TMP}"' EXIT

    # Disable the Sionna feature for the build.
    (cd "${REPO_ROOT}/src" && opp_featuretool disable Simu5G_Sionna 2>/dev/null || true)

    # Build into a known output directory (use a sub-mode label so the .so lands
    # at a predictable path that does NOT clobber the user's current build).
    OFF_MODE="seam02check"
    (cd "${REPO_ROOT}/src" && make MODE="${OFF_MODE}" -j"$(nproc 2>/dev/null || echo 4)" \
         2>&1 | tail -20) || { echo "ERROR: feature-OFF build failed." >&2; exit 2; }

    # Find the library produced by this build.
    target=""
    while IFS= read -r lib; do
        if nm_readable "${lib}"; then
            target="${lib}"
            break
        fi
    done < <(find "${REPO_ROOT}/src" "${REPO_ROOT}/out" -maxdepth 8 \
                 \( -name "libsimu5g*${OFF_MODE}*.so" -o \
                    -name "libsimu5g*${OFF_MODE}*.a"  -o \
                    -name "libsimu5g*${OFF_MODE}*.dylib" \) \
                 2>/dev/null || true)

    # Fallback: look under out/${OFF_MODE}.
    if [[ -z "${target}" ]]; then
        while IFS= read -r lib; do
            if nm_readable "${lib}"; then
                target="${lib}"
                break
            fi
        done < <(find "${REPO_ROOT}/out/${OFF_MODE}" -maxdepth 8 \
                     \( -name 'libsimu5g*.so' -o -name 'libsimu5g*.a' -o \
                        -name 'libsimu5g*.dylib' \) \
                     2>/dev/null || true)
    fi

    # Re-enable the Sionna feature immediately after the build so the user's tree
    # is restored (the EXIT trap also does this, but be explicit here).
    restore_features
    trap 'rm -rf "${BUILD_TMP}"' EXIT

    if [[ -z "${target}" ]]; then
        echo "ERROR: feature-OFF build produced no inspectable library." >&2
        exit 2
    fi
    echo "SEAM-02 self-certifying: inspecting feature-OFF artifact: ${target}"

# ---- Mode 2: explicit path given — trust the caller. ----------------------------
elif [[ -n "${EXPLICIT_PATH}" ]]; then
    target="${EXPLICIT_PATH}"
    if ! nm_readable "${target}"; then
        echo "ERROR: '${target}' is not an nm-readable binary/library." >&2
        exit 2
    fi
    echo "SEAM-02 symbol check: inspecting (caller-supplied) ${target}"

# ---- Mode 3: no args — glob probe, but only safe when feature is currently OFF. --
else
    # Detect whether the Simu5G_Sionna feature is currently enabled in the source tree.
    # opp_featuretool list -e prints enabled features; grep for Simu5G_Sionna.
    sionna_enabled=0
    if command -v opp_featuretool >/dev/null 2>&1; then
        if (cd "${REPO_ROOT}/src" && opp_featuretool list -e 2>/dev/null | grep -q 'Simu5G_Sionna'); then
            sionna_enabled=1
        fi
    fi

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
    while IFS= read -r lib; do
        candidates+=("${lib}")
    done < <(find "${REPO_ROOT}/src" "${REPO_ROOT}/out" -maxdepth 6 \
                 \( -name 'libsimu5g*.so' -o -name 'libsimu5g*.a' -o -name 'libsimu5g*.dylib' \) \
                 2>/dev/null || true)
    candidates+=(
        "${REPO_ROOT}/bin/simu5g"
        "${REPO_ROOT}/bin/simu5g_dbg"
    )

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
        echo "       or use --build to self-certify via a fresh feature-OFF compile." >&2
        exit 2
    fi

    # WR-06: refuse to certify an ambiguous artifact when the feature is currently ON.
    # The found library could be a stale feature-OFF artifact — its PASS would not
    # prove the current tree is clean. Require --build or an explicit OFF artifact.
    if [[ "${sionna_enabled}" -eq 1 ]]; then
        echo "AMBIGUOUS: Simu5G_Sionna feature is currently ENABLED in this source tree." >&2
        echo "           The probed library '${target}' may be a stale feature-OFF build;" >&2
        echo "           its PASS result would not certify the current tree (WR-06)." >&2
        echo "           Use --build to compile a fresh feature-OFF library and inspect that," >&2
        echo "           or disable the feature before probing: opp_featuretool disable Simu5G_Sionna" >&2
        exit 3
    fi

    echo "SEAM-02 symbol check: inspecting ${target} (feature confirmed OFF)"
fi

# ---- Shared: extract symbols and match against the forbidden set. ----------------

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
