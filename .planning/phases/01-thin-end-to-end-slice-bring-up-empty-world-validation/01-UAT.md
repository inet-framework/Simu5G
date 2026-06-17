---
status: testing
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
source: [01-VERIFICATION.md]
started: 2026-06-17
updated: 2026-06-17
---

## Current Test

number: 1
name: In-sim path gain reflects the Sionna table (not the analytic 3GPP model)
expected: |
  The single-link empty-world run uses SionnaChannelModel (selected via the nrChannelModelType
  ini string), and the in-sim per-link path gain tracks the offline Sionna/Friis value
  (~-83.3 dB at 100 m) within ~1 dB — NOT the analytic 3GPP path loss. Operator-observed
  rcvdSinrDl:mean = 70.14 dB is consistent with the -83.3604 dB Sionna table value.
awaiting: user response

## Tests

### 1. In-sim path gain = Sionna table value
expected: rcvdSinrDl (or logged getAttenuation) tracks the offline friis_check.py value within ~1 dB and is NOT the analytic 3GPP path loss. Operator observed rcvdSinrDl:mean = 70.14 dB during the 01-04 run, consistent with -83.3604 dB Sionna path gain.
result: [pending — operator approved live at the 01-04 checkpoint; persistent scalar (.sca) not committed, only 0.vec/0.vci]

### 2. Corrupted-manifest negative check aborts with cRuntimeError
expected: Editing manifest carrier_frequency_hz to a wrong value and re-running aborts nonzero with a message naming the mismatch.
result: [independently re-confirmed — /tmp/sionna_run_neg.log: "Error: Sionna manifest carrier_frequency_hz mismatch: artifact 2.6e+09 Hz, scenario 3.5e+09 Hz -- in module (simu5g::SionnaManager) ... during network initialization". Also operator-approved at the 01-04 checkpoint.]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps
