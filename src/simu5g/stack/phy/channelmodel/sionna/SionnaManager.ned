//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.channelmodel.sionna;

import simu5g.stack.phy.channelmodel.ISionnaManager;

//
// SionnaManager
//
// Global owner of the Sionna ray-traced channel table (Plan A): a single, network-wide
// module (like ~Binder / ~CarrierAggregation) that holds the per-(link, RB) path gains
// which every per-PHY ~SionnaChannelModel looks up. At most one instance per network.
//
// Enabling it: the standard networks carry an opt-in submodule
//   sionnaManager: <default("SionnaManager")> like ISionnaManager if hasSionnaManager
// so set "*.hasSionnaManager = true" and switch the channel model to SionnaChannelModel
// (or CompareChannelModel). The channel models locate it through their
// "sionnaManagerModule" parameter (default "sionnaManager"). Worked examples and a
// shared ini live under simulations/nr/sionna/.
//
// At a late init stage (positions final, all participating channel models have
// self-registered) it enumerates the nodes and component carriers, builds a request,
// and obtains the table; it rebuilds while the node set is still growing during init
// and freezes the result at the end of initialization.
//
// SCOPE: static scenarios only (v1). A node that moves leaves the position-keyed table
// and the lookup errors out.
//
// =========================== Usage modes ===========================
//
// (1) Committed artifact  -- portable & deterministic; needs NEITHER Python NOR Sionna.
//     Loads a frozen JSON table that was produced once and checked in; no subprocess is
//     ever spawned and the cache is bypassed. The granularity / interferenceMode recorded
//     in the file header are authoritative. This is the recommended mode for committed
//     scenarios and fingerprint tests.
//        channelTableFile = "channel_table.json"   // path, relative to the run directory
//
// (2) Live generation, two-ray backend  -- deterministic, no GPU, stdlib-only Python.
//     Spawns sionna_rt.py to compute a flat-ground two-ray (direct + ground-reflected)
//     table at startup, cached under cacheDir by a hash of the request (scene, positions,
//     materials, carriers, modes); a rerun with the same scenario reuses the cache.
//     Use it to (re)generate the artifact for mode (1), or for quick static studies.
//        channelTableFile = ""           // empty -> generate (do not load an artifact)
//        backend          = "tworay"     // or "auto" (uses sionna if importable, else tworay)
//        pythonExecutable = "python3"     // stdlib only -> the system python is fine
//        sionnaScript     = ""           // empty -> the bundled sionna_rt.py (next to the NED)
//        cacheDir         = "sionna_cache"
//        forceRegenerate  = false         // true -> ignore an existing cache entry
//
// (3) Live generation, real Sionna RT  -- site-specific ray tracing (NVIDIA Sionna RT).
//     As mode (2) but runs actual ray tracing; requires a Python that can import
//     sionna.rt (Sionna RT = Mitsuba 3 + Dr.Jit, CPU/LLVM or GPU). Results may vary
//     slightly across machines (float/GPU), so for stable fingerprints freeze the output
//     and load it via mode (1).
//        channelTableFile = ""
//        backend          = "sionna"
//        pythonExecutable = "/path/to/venv/bin/python3"   // must import sionna.rt
//        sceneFile        = "scene.xml"   // optional Mitsuba 3 scene (see "Scene" below)
//
// ======================= Cross-cutting settings =======================
//
// Granularity (granularity):
//   "perRb"     -- one path gain per logical band (matches Simu5G's per-band reception).
//   "wideband"  -- a single value per link.
//
// Interference (interferenceMode):
//   "noiseLimited" -- serving links only; SINR = SNR (no inter-cell interference table).
//   "allPairs"     -- full Tx->Rx matrix; required for accurate multi-cell interference.
//   Coupling guard: "wideband" + "allPairs" is inconsistent (a single value cannot carry
//   RB-selective interference) and the module warns; prefer "perRb" for multi-cell.
//   (Background/external cells are out of scene scope and are not modelled by Sionna.)
//
// Scene (modes 2 and 3):
//   sceneFile == "" -- flat ground plane: a direct path plus a ground reflection (NOT
//                      pure free space), set by groundPermittivity / groundConductivity /
//                      sceneSize / numReflections / polarization.
//   sceneFile != "" -- a Sionna-native Mitsuba 3 XML scene; node positions must already
//                      be expressed in THAT scene's coordinate frame (user's
//                      responsibility in v1). Real geometry integration is future work.
//
// Wiring: binderModule / carrierAggregationModule point at the network-level Binder and
// CarrierAggregation used to enumerate the nodes and carriers.
//
simple SionnaManager like ISionnaManager
{
    parameters:
        @display("i=block/server");
        @class("SionnaManager");

        string binderModule = default("binder");
        string carrierAggregationModule = default("carrierAggregation");

        // Scene (Plan A v1: flat ground). When sceneFile is set, it is a
        // Sionna-native Mitsuba 3 XML scene and node positions must be expressed
        // in that scene's coordinate frame (user's responsibility in v1).
        string sceneFile = default("");
        double groundPermittivity = default(5.0);     // relative permittivity of the ground
        double groundConductivity = default(0.001);   // [S/m]
        double sceneSize @unit(m) = default(2000m);   // flat-ground half-extent
        int numReflections = default(1);              // ground reflections enabled (>=1)
        string polarization @enum(vertical,horizontal) = default("vertical");

        // Per-RB (matches Simu5G's per-band reception) vs. one wideband value.
        string granularity @enum(perRb,wideband) = default("perRb");
        // Serving links only (SINR = SNR) vs. full Tx->Rx matrix.
        string interferenceMode @enum(noiseLimited,allPairs) = default("noiseLimited");

        // Channel-table artifact handling.
        // If channelTableFile is non-empty, it is loaded directly (the committed,
        // fingerprint-stable artifact) and no subprocess is ever spawned.
        string channelTableFile = default("");
        string cacheDir = default("sionna_cache");
        bool forceRegenerate = default(false);

        // Subprocess generator (used only on a cache miss with no channelTableFile).
        // "python3" suffices for the tworay backend (stdlib only); for backend="sionna"
        // set this to a python that can import sionna.rt.
        string pythonExecutable = default("python3");
        // path to sionna_rt.py; empty -> the bundled one, found next to this NED file
        // (via resolveResourcePath, which also searches the cwd / ini dir / NED path).
        string sionnaScript = default("");
        string backend @enum(auto,sionna,tworay) = default("auto");
}
