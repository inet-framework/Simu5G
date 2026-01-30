# Fingerprint tests

## Using the fingerprint tests

- Use the provided "fingerprint test" suite for testing. The command is this:
  `(cd tests/fingerprint/ && ./fingerprints -d)`. It runs all simulations in the
  project (in `examples/`, `showcases/`, etc) for a configured time limit, and
  verifies that they produce the same simulation fingerprints as the recorded
  ones. Runtime errors and fingerprint validation failures are reported.
  Simulations and fingerprints are stored in csv files (`simulations.csv`). See
  OMNeT++ manual for what simulation fingerprints are. Running the full fingerprint
  test takes about a few minutes on a contemporary laptop.

- Filtering: Add `-m <substring>` args to filter for a subset of simulations that
  include any of the substrings (in the config name, directory name, or elsewhere).
  Such partial fingerprint tests are useful for saving time between intermediate
  refactoring steps, for example. (After completing the refactoring, running the
  full fingerprint test suite is required.)

- Errors and fingerprint failures are NOT ACCEPTABLE (except when expected).

## What are fingerprints?

- FINGERPRINTS: 32-bit integer, computed by taking the specified "ingredients"
  into account. Ingredients are the parts after the slash. E.g.:
  `ce26-3758/tplx;5d56-5fa2/~tNl;2251-2948/sz`  contains 3 fingerprints
  (separated by semicolons), and the letters in `tplx`, `~tNl`, `sz` are all
  "ingredients". `t` is simulation time, `p` is module path, `l` is packet
  length, `s` is scalar results, etc. For more info on fingerprints mechanism
  (multiple fingerprints, fingerprint ingredients, etc), see the OMNeT++ manual.

## Fingerprint ingredients recognized by OMNeT++

See Testing chapter in the OMNeT++ manual.

  - "e": event number
  - "t": simulation time
  - "n": message (event) full name
  - "c": message (event) class name
  - "k": message kind
  - "l": message (packet) bit length
  - "o": message control info class name
  - "g": message control info (uses parsimPack())
  - "d": message data (legacy, DO NOT USE)* (uses parsimPack())
  - "b": message contents (uses parsimPack())
  - "i": module id
  - "m": module full name (name with index)
  - "p": module full path (hierarchical name)
  - "a": module class name
  - "r": random numbers drawn
  - "s": scalar results (just the data, without name or module path)
  - "z": statistic results: histogram, statistical summary (just the data, without name or module path)
  - "v": vector results (just the data, without name or module path); has no effect if vector recording is disabled
  - "y": display strings of all modules (on refreshDisplay())
  - "f": essential properties (geometry) of canvas figures (on refreshDisplay())
  - "x": extra data provided by modules
  - "0": clean hasher

## Fingerprint ingredients added by INET:

These are implemented in the inet::FingerprintCalculator class.

  - "~"	NETWORK_COMMUNICATION_FILTER: Excludes intra-node communication; only
    includes packet arrivals crossing different network nodes.

  - "U"	PACKET_UPDATE_FILTER: Excludes packet update arrival events (e.g.,
    transmission truncations) from fingerprint calculation.

  - "N"	NETWORK_NODE_PATH: Adds full paths of sender and arrival network nodes for
    packet arrival events.

  - "I"	NETWORK_INTERFACE_PATH: Adds full interface paths of sender and arrival
    network interfaces for packet arrival events.

  - "D"	PACKET_DATA:	Includes raw packet data bytes/bits in the fingerprint for
    non-zero-length packets. Requires packet serialization, and forces checksum
    computation. It cannot be used if the packet contains chunks that do not
    have serializers.

## Typical fingerprint ingredient combinations:

  - "tplx": Simulation time, module path, and packet length. Includes module
    path, so it is sensitive to module renames, but NOT to the addition or
    deletion of event-less modules and module relocations, submodule order
    changes. To validate module renames, use "tilx".

  - "tilx": Simulation time, module ID, and packet length. Uses module ID
    instead of module path, so it is insensitive to module renames, but it is
    sensitive to changes in model structure: adding/deleting or relocating
    modules, and also to adding/deleting connections with channel objects
    (channel objects also occupy IDs).

  - "sz": Scalar and histogram/statistical summary results (ignores vector
    results). The hash itself does not explicitly include the module path or
    statistic name, just the numerical value. It is sensitive the
    addition/deletion of statistics (so it may also easily change when
    switching to a different INET version). It is also sensitive to changes in
    the order results are recorded (e.g. to changes in module order).

  - "tyf": Graphical fingerprint: display strings and figure properties. It is
    useful for testing model visualization. Since display strings and other
    visualizations (e.g. canvas figures) are usually updated from
    updateDisplay() methods in the model, the fingerprint runner test will turn
    on Cmdenv's "fake GUI" feature for these fingerprints, that generates
    updateDisplay() calls in the model.

  - "~tNl": Network-level traffic without packet data (packet lengths only). It
    is insensitive to events inside network nodes: as long as the network-level
    traffic stays the same, the fingerprint will not change. It is useful for
    validating restructuring changes inside network nodes.

  - "~tND": Network-level traffic including packet data. This one is like
    "~tNl", but stricter, and also has a larger runtime cost. It works via
    packet serialization. The fingerprint runner script automatically enables
    computed checksums/CRCs/FCS for this fingerprint, as it is needed for
    serialization. It does not work if some packet chunk does not have a
    serializer.


