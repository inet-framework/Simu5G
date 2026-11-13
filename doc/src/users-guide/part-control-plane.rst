Control Plane and Procedures
============================

Simu5G models the control plane through its entities: the RRC in every
cellular NIC, the core network control function, and the control of the user
plane nodes. The procedures run between these entities as direct calls named
after the 3GPP messages they stand for, and where the duration of a procedure
matters, it is modeled with timers. The relevant specifications are 3GPP TS
38.331 and TS 36.331 (RRC), TS 23.502 (the procedures of the 5G system), TS
37.340 (multi-connectivity), and TS 36.423 (X2AP).

The Control Plane chapter describes the entities, attach and session
establishment. Handover, Dual Connectivity, and X2 and CoMP describe the
procedures and features built on them.

.. toctree::
   :maxdepth: 1

   control-plane
   handover
   dual-connectivity
   x2
