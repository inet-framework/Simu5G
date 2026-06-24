# Report: Simu5G's cross-model shadowing/fading coupling and `CompareChannelModel`

Status: description / lesson learned
Related commit: `6a17f786` — *Sionna: make CompareChannelModel transparent (re-parent to realistic model)*
Related plan: [Plan A §7](sionna-channel-plan.md) (validation decorator)

## TL;DR

When computing a DL CQI, Simu5G's built-in channel model (`LteRealisticChannelModel`
and its subclasses) **reaches into the peer (UE) channel model** for the shadowing and
Jakes-fading state, and to do so it `dynamic_cast`s the polymorphic `LteChannelModel*`
pointer down to the concrete analytic type
(`dynamic_cast<LteRealisticChannelModel*>`). This is a **leaky abstraction / tight
coupling**: the analytic internal state is not part of the channel-model interface,
yet it must be reached from another instance.

Because of this, a *decorator* channel model (`CompareChannelModel`) deriving from
`LteChannelModel` got **null** from the cast → **segfault / throw**. The fix:
re-parent the decorator to `LteRealisticChannelModel` and forward the map accessors
to the inner reference model.

---

## 1. The mechanism that justifies the cast

Shadowing and Jakes fading are **stateful and UE-specific**:

- **Shadowing:** time-correlated (Exponential Average Window), stored per UE in a
  `ShadowFadingMap` that evolves over the run.
- **Jakes fading:** the angles of arrival are initialized **once** per UE from random
  draws (`uniform`, `exponential`), then it is a deterministic function of time.

This state lives in the **UE-side** channel-model instance
(`lastComputedSF_`, `jakesFadingMap_`).

The source of the conflict: the **DL CQI is computed at the eNB** (its `getSINR` runs
on the eNB's own channel model), but the DL-direction shadowing/fading is **what the
UE experiences**. For the eNB's CQI estimate to be **consistent** with what the UE
will actually see at reception, the eNB must use the **UE's map**. The code comment
states exactly this ([LteRealisticChannelModel.cc:196](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L196)):

```cpp
double LteRealisticChannelModel::computeShadowing(..., bool cqiDl)
{
    ShadowFadingMap *actualShadowingMap;
    if (cqiDl) // if we are computing a DL CQI we need the Shadowing Map stored on the UE side
        actualShadowingMap = obtainShadowingMap(nodeId);   // <- reach into the peer
    else
        actualShadowingMap = &lastComputedSF_;             // own (UL / own side)
    ...
}
```

## 2. Where and how it downcasts

The peer state is reached through the polymorphic `getChannelModel()`, which returns
the base `LteChannelModel*` — but `getShadowingMap()`/`getJakesMap()` only exist on
`LteRealisticChannelModel` (the analytic model's internal state), so it must be
**downcast**:

[LteRealisticChannelModel.cc:2577-2595](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L2577) — `obtainShadowingMap`:
```cpp
LtePhyBase *phy = <UE phy from the Binder, by ueId>;
if (phy == nullptr) return nullptr;
LteRealisticChannelModel *re = dynamic_cast<LteRealisticChannelModel*>(phy->getChannelModel(carrierFrequency_));
ShadowFadingMap *j = re->getShadowingMap();   // <- NO null-check on re!
return j;
```

[LteRealisticChannelModel.cc:2551-2575](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L2551) — `obtainUeJakesMap` (this one checks explicitly):
```cpp
LteRealisticChannelModel *re = dynamic_cast<LteRealisticChannelModel*>(phy->getChannelModel(carrierFrequency_));
if (re == nullptr)
    throw cRuntimeError("... channel model is a null pointer");
return re->getJakesMap();
```

The same downcast pattern also appears in the **interference computation**
([LteRealisticChannelModel.cc:2629](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L2629)):
it casts a neighbouring cell's `phy->getChannelModel(...)` to
`LteRealisticChannelModel*` to call its `getAttenuation()`. So the coupling is **not a
one-off**: the built-in model assumes at several points that the *peer's* channel
model is an `LteRealisticChannelModel`.

## 3. How it manifested (the symptom)

The Plan A §7 `CompareChannelModel` decorator is the UE's channel model. When the
eNB's built-in model computed a DL CQI and reached for the UE's shadowing/jakes map:

```cpp
re = dynamic_cast<LteRealisticChannelModel*>(<the UE's CompareChannelModel>);
// CompareChannelModel : public LteChannelModel  →  re == nullptr
re->getShadowingMap();   // obtainShadowingMap: null-deref → SEGFAULT
                         // obtainUeJakesMap : explicit throw
```

The crash appeared during the init phase (cell-attach DL CQI), in
`...ue[*].cellularNic.nrPhy`. We previously worked around it in the
`CompareChannelModel` scenario by **disabling** the built-in shadowing/fading (the §7
"large-scale only" mode) — but that changed the physics away from the built-in
baseline, so the fingerprint did not match (FAILED, not PASS).

## 4. The fix we applied

To make the decorator *transparent* (fading ON, matching the built-in baseline), we
conformed to the existing coupling "contract":

1. **Make the two accessors virtual** in `LteRealisticChannelModel`
   ([LteRealisticChannelModel.h:396,401](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h#L396)):
   ```cpp
   virtual JakesFadingMap   *getJakesMap()     { return &jakesFadingMap_; }
   virtual ShadowFadingMap  *getShadowingMap() { return &lastComputedSF_; }
   ```
2. **Derive the decorator from `LteRealisticChannelModel`**
   ([CompareChannelModel.h:26](../../src/simu5g/stack/phy/channelmodel/CompareChannelModel.h#L26)) →
   the `dynamic_cast` **succeeds** (non-null).
3. **Forward the two accessors to the inner reference model**
   ([CompareChannelModel.h:70,80](../../src/simu5g/stack/phy/channelmodel/CompareChannelModel.h#L70)):
   ```cpp
   ShadowFadingMap *getShadowingMap() override {
       if (reference_) { auto *r = dynamic_cast<LteRealisticChannelModel*>(reference_.get());
                         if (r) return r->getShadowingMap(); }
       return LteRealisticChannelModel::getShadowingMap();
   }
   ```
   This way the eNB and the UE-side reference **share one consistent map**, just like
   in a plain built-in run.

**Result (proven):** `SionnaCompare` vs. a plain built-in `BuiltinRef` twin — **tplx
and ~tNl match bit-for-bit** (`55bf-1704` / `b62a-c0f4`); only **sz** differs (the
logged Sionna deltas). Full fingerprint suite: 129 OK. Making the base accessors
virtual is **inert** for the other tests (no behavioral change).

## 5. Trade-offs and cleaner alternatives

The fix is **conforming** to the existing coupling, not removing it. Its downsides:

- The decorator **carries** a full `LteRealisticChannelModel` (shadowing/jakes
  members, interference routines, the `getSINR` impl) of which it uses almost nothing
  (it forwards everything to the two inner models).
- **The base class had to be touched** (2 accessors made virtual).

Cleaner (but more invasive) options, should the coupling ever be refactored:

1. **Expose the maps on the interface:** move `getShadowingMap()`/`getJakesMap()` (or
   a narrower "large-scale state" interface) up to `ILteChannelModel` /
   `LteChannelModel` → no downcast, the decorator forwards cleanly.
2. **Explicit state passing:** have the DL CQI computation receive the UE's
   large-scale state as a parameter, instead of reaching into the peer instance —
   removing the hidden inter-instance dependency.
3. **Externalize the state** into a separate, channel-model-independent store (e.g. in
   the `Binder` or a "large-scale state" service) that both endpoints access the same
   way — so the model itself could be a stateless lookup.

## 6. Lesson

The `dynamic_cast` is not arbitrary: it serves a real **consistency requirement** (the
DL CQI must use the same correlated shadowing/fading realization the UE sees) through a
**leaky abstraction**. Any *decorator or alternative* channel model that sits at one
end of a link while the other end runs the built-in model **will hit** this — either
conform to it (as we did), or work around it by disabling the built-in's large-scale
effects. When designing a new channel model, account for this coupling up front (at
least 3 downcast sites in the code: `obtainShadowingMap`, `obtainUeJakesMap`,
`computeDownlinkInterference`).
