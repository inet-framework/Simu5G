---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: planning
stopped_at: Plan A pivot — roadmap restructured (channel track Phases 2–4; BLER parked to Phase 5)
last_updated: "2026-06-18T13:35:09.751Z"
last_activity: 2026-06-18 — Plan A (channel) pivot: PROJECT/REQUIREMENTS/ROADMAP restructured; BLER Phase 2 archived to .planning/parked/
progress:
  total_phases: 5
  completed_phases: 1
  total_plans: 4
  completed_plans: 4
  percent: 20
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-06-17)

**Core value:** Simu5G can opt in to site-specific, geometry-derived channel/BLER from Sionna RT without changing the default build or behavior.
**Current focus:** Phase 2 — Channel Source & Format Maturation (Plan A channel track)

## Current Position

Phase: 2 (Channel Source & Format Maturation)
Plan: Not started — ready to discuss/plan
Status: Plan A pivot complete; channel track is active, BLER parked to Phase 5
Last activity: 2026-06-18

Progress: [██░░░░░░░░] 20% (Phase 1 complete; Phase 2 channel track not started)

## Performance Metrics

**Velocity:**

- Total plans completed: 4
- Average duration: — min
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 4 | - | - |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*
| Phase 01 P01 | 3 | 3 tasks | 7 files |
| Phase 01 P02 | 3 | 3 tasks | 11 files |
| Phase 01 P03 | 8 | 3 tasks | 10 files |
| Phase 01 P04 | 35min | 4 tasks | 2 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: Vertical MVP framing — each phase is an end-to-end slice; Phase 1 is a thin single-link bring-up validated by the empty-world Friis round-trip.
- Roadmap: `schema_version` + `coord_transform` contract and fail-loud parameter assertion ship in minimal form inside Phase 1 (foundational; every later slice extends them).
- Roadmap: v1 noise-limited table designed as a strict subset of v2 via a present-but-degenerate SINR-bin axis from Phase 1.
- [Phase ?]: Plan 01-01: empty-world Sionna path gain matches Friis within 0.006 dB at 100m/3.5GHz, validating the TOOL-02 coord_transform and dB convention.
- [Phase ?]: Plan 01-02: Simu5G_Sionna .oppfeatures feature (extraSourceFolders) is the SEAM-02 build-isolation mechanism keeping the default build unaffected; the sionna C++ skeleton compiles only when the feature is ON.
- [Phase ?]: 01-03: Reused in-tree nlohmann/json 3.9.1 (MEC httpUtils) for Sionna manifest parsing — no new vendor, zero default-build symbols.
- [Phase ?]: 01-03: Contract assertion is a pure static method so fail-loud CAL-02 logic is unit-testable without the OMNeT++ kernel.
- [Phase ?]: 01-03: Added standalone C++ unit harness (tests/sionna/unit) for real RED/GREEN TDD of the Sionna pure utilities.
- [Phase ?]: Sionna config fingerprint baselines are the config's own (REP-01), captured via fingerprinttest.py for format parity
- [Phase ?]: SEAM-02 gate matches demangled symbols (identifier-boundary Sionna + substring hdf5|python|tensorflow|torch); probes libsimu5g*.so since bin/simu5g is a launcher script

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 3 (planning audit): audit the full `LteRealisticChannelModel` inherited call tree to find every statistical path-loss term before suppressing them.
- Phase 1 (empirical gate): the ~0.5–1 dB Friis-vs-Sionna tolerance must be confirmed on the first real empty-world run.

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-06-18T13:35:09.745Z
Stopped at: Phase 2 context gathered
Resume file: .planning/phases/02-full-bler-table-cqi-feedback-consistency/02-CONTEXT.md
