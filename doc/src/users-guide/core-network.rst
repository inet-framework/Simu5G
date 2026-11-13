The Core Network
================

.. TODO: This chapter covers: Upf, PgwStandard, CoreControl, UserPlaneRelay, GTP, TrafficFlowFilter, TEIDs, Ipv6NdResponder, EthernetSessionBridge.

Core network nodes
------------------

``Upf`` models a 5G User Plane Function. It is derived from an INET
router-like base (``ApplicationLayerNodeBase``), so it can be equipped with
any number of wired interfaces. Two submodules give it its UPF role: the
``trafficFlowFilter`` and ``gtp_user`` pair performs GTP tunneling, and a
dedicated PPP interface (``dnPpp``) connects it to the data network. The
same module type serves two purposes:

*  as the *PDU Session Anchor* -- the entry/exit point of the mobile
   network, terminating GTP tunnels towards the data network;
*  as an *intermediate UPF* (iUPF), in which case it simply forwards
   tunneled traffic like an IP router.

``PgwStandard`` is the 4G equivalent (the PDN Gateway of the EPC), with
the same structure.
