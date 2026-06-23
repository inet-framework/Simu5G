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
// Global, network-wide owner of the Sionna ray-traced channel table (Plan A).
// There must be at most one instance per network (like Binder/CarrierAggregation).
//
// At the end of initialization (positions final, all nodes registered with the
// Binder) it enumerates every node and component carrier, builds a request, and
// obtains the per-(link, RB) path-gain table either by loading a cached/committed
// JSON artifact or by spawning the ray-tracing generator (sionna_rt.py) as a
// subprocess. Per-PHY ~SionnaChannelModel instances are thin lookups into this
// shared table.
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
        string pythonExecutable = default("python3");
        string sionnaScript = default("");   // path to sionna_rt.py; required to spawn
        string backend @enum(auto,sionna,tworay) = default("auto");
}
