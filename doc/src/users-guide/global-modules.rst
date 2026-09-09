Global Modules
==============

Overview
--------

A few Simu5G modules exist once per network and provide information or
services to all cellular nodes. They are plain modules placed at the top
level of the network description; stack modules find them through
module-path parameters (``binderModule``, ``bearerConfiguratorModule``,
etc.) rather than hardcoded paths, so several independent cellular networks
can in principle coexist in one simulation.

The Binder
----------

The ``Binder`` (``src/simu5g/common/binder/``) is the directory of the
cellular network. It assigns and resolves the node identifier space (the
"MAC node IDs" used throughout the stack), and records:

*  which UE is attached to which cell,
*  master/secondary relationships for dual connectivity,
*  X2 peerings between base stations,
*  D2D multicast group membership.

Modules query the Binder at runtime -- for example, a receiving PHY asks it
which other cells transmit on the same carrier in order to compute
inter-cell interference. The Binder is pure infrastructure: it has almost no
user-facing parameters, and a simulation user normally only needs to know
that it must be present, and that a node's *node id* (as used in
``servingNodeId`` or ``masterId``) is assigned by it in order of
registration.

.. TODO: describe how node ids are assigned (numbering scheme for eNBs/gNBs
   and UEs) -- users need this to set servingNodeId/masterId confidently.

The BearerConfigurator
----------------------

The ``BearerConfigurator`` (``src/simu5g/corenetwork/bearerConfigurator/``)
is the single authority for bearer and QoS configuration: which data radio
bearers exist or may be created on demand, what their QoS characteristics
are, and how packets are classified onto QoS flows. Its configuration is
expressed in a few JSON-style object parameters (``staticDrbs``,
``onDemandDrbs``, ``drbProfiles``, ``dlQfiRules``, ``ulQfiRules``) and is
delivered to the evaluation sites in the network at initialization. It is
the subject of :doc:`bearers`.

The CarrierAggregation module
-----------------------------

The ``CarrierAggregation`` module (``src/simu5g/common/carrierAggregation/``)
describes the radio spectrum of the network as a vector of
``ComponentCarrier`` submodules, each with its own carrier frequency, number
of resource blocks, numerology and duplexing settings. Base stations and UEs
refer to these carriers; per-carrier configuration is described in
:doc:`carriers`.

Channel control
---------------

``LteChannelControl`` (``src/simu5g/world/radio/``) keeps a registry of
all radio-capable modules and their positions, so that transmissions can be
delivered to the modules they may affect. It plays the same role INET's
radio medium module plays for INET wireless models. Exactly one instance is
required per network; its parameters rarely need to be changed.

Notes on module references
--------------------------

All references to global modules from inside nodes are parameters -- e.g.
every stack module has a ``binderModule`` parameter defaulting to
``"binder"``. If you name the global modules differently in your network, or
run multiple cellular networks side by side, set these parameters
accordingly.
