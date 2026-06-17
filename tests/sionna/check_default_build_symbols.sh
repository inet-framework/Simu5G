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
FORBIDDEN_PATTERN='sionna|hdf5|python|tensorflow|torch'

# Resolve the repository root from this script's location so the probe works
# regardless of the caller's current working directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Build the list of candidate binaries/libraries to inspect.
candidates=()
if [[ $# -ge 1 && -n "${1:-}" ]]; then
    candidates+=("$1")
else
    # Common Simu5G default output locations (release + debug, exe + shared lib).
    candidates+=(
        "${REPO_ROOT}/bin/simu5g"
        "${REPO_ROOT}/bin/simu5g_dbg"
    )
    # Shared/static libraries produced by opp_makemake (path varies by mode/arch).
    while IFS= read -r lib; do
        candidates+=("${lib}")
    done < <(find "${REPO_ROOT}/src" "${REPO_ROOT}/out" -maxdepth 6 \
                 \( -name 'libsimu5g*.so' -o -name 'libsimu5g*.a' -o -name 'libsimu5g*.dylib' \) \
                 2>/dev/null || true)
fi

# Pick the first candidate that actually exists on disk.
target=""
for c in "${candidates[@]}"; do
    if [[ -e "${c}" ]]; then
        target="${c}"
        break
    fi
done

if [[ -z "${target}" ]]; then
    echo "ERROR: no default Simu5G binary/library found to inspect." >&2
    echo "       Build the default target first (e.g. 'make' with Simu5G_Sionna OFF)," >&2
    echo "       or pass an explicit path: $0 <binary-or-library>" >&2
    exit 2
fi

echo "SEAM-02 symbol check: inspecting ${target}"

# Extract symbol names. Prefer dynamic symbols (-D); fall back to the full table
# (static archives / stripped-dynamic objects yield nothing from 'nm -D').
symbols="$(nm -D "${target}" 2>/dev/null || true)"
if [[ -z "${symbols}" ]]; then
    symbols="$(nm "${target}" 2>/dev/null || true)"
fi

if [[ -z "${symbols}" ]]; then
    echo "ERROR: could not read any symbols from ${target} (nm produced no output)." >&2
    exit 2
fi

# Case-insensitive match against the forbidden patterns.
matches="$(printf '%s\n' "${symbols}" | grep -iE "${FORBIDDEN_PATTERN}" || true)"

if [[ -n "${matches}" ]]; then
    echo "FAIL: forbidden symbols found in the default build (${target}):" >&2
    printf '%s\n' "${matches}" >&2
    echo "SEAM-02 violated: the default build is NOT free of Sionna/HDF5/Python symbols." >&2
    exit 1
fi

echo "PASS: ${target} contains no sionna/hdf5/python/tensorflow/torch symbols (SEAM-02 OK)."
exit 0
