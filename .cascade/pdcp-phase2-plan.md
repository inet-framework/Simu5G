# PDCP Phase 2: Split PdcpMux into UpperMux + LowerMux + DcMux

## Target architecture

```
compound PdcpLayer {
    submodules:
        upperMux   // dispatches upper→TX, collects RX→upper
        lowerMux   // dispatches RLC→RX, collects TX→RLC (+ NR RLC routing)
        dcMux      // dispatches DC/X2 ↔ bypass entities (conditional)
        // entities created dynamically with in/out gates
}
```

Each mux has dual roles: fan-out (dispatch by DrbKey) and fan-in (collect from
entities and forward to a single output gate).

## Data flows

- TX: upperLayerIn → upperMux → txEntity.in → txEntity.out → lowerMux → rlcOut
- RX: rlcIn → lowerMux → rxEntity.in → rxEntity.out → upperMux → upperLayerOut
- NR: lowerMux routes to rlcOut vs nrRlcOut based on tag from entity
- DC bypass TX: dcManagerIn → dcMux → bypassTx.in → bypassTx.out → lowerMux → rlcOut
- DC bypass RX: rlcIn → lowerMux → bypassRx.in → bypassRx.out → dcMux → dcManagerOut

## Implementation steps

### Step 7: Add in/out gates to entities + handleMessage
Add `in` and `out` gates to all entity NED types. Add `handleMessage` that
delegates to existing handler methods. Gates are unconnected at this point.

### Step 8: Create UpperMux and LowerMux
Simple modules with dynamic gate arrays for entity connections. Move dispatch
logic from PdcpMux. Wire in PdcpLayer.ned.

### Step 9: Create DcMux
Handles DC/X2 traffic. Only instantiated when DC is enabled.

### Step 10: Wire everything, remove old PdcpMux

### Step 11: Remove IPdcpGateway interface
Entities send on `out` gate instead of calling method-based forwarding.
