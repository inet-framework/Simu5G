System Architecture
===================

Simu5G follows the 3GPP system architecture: its network nodes are the UE, the
base stations (eNB and gNB) and the core network functions, and inside every
cellular node, the cellular NIC carries the radio protocol stack. This part
describes that architecture as Simu5G models it. It corresponds to 3GPP TS
23.501 (the 5G system architecture), TS 38.300 and TS 36.300 (the overall
description of NR and E-UTRA), and TR 38.801 for the deployment options.

The Architecture Overview chapter maps the 3GPP entities to Simu5G modules and
explains how the user plane and the control plane are modeled. Deployment
Options shows how complete networks are assembled for SA, LTE and dual
connectivity. The chapters on the UE, the base station and the core network
describe the node types, and The Cellular NIC describes the compound that all
cellular nodes share. The later parts refer back to these chapters, so it is
best to read this part first.

.. toctree::
   :maxdepth: 1

   architecture
   deployments
   ue
   base-station
   core-network
   cellular-nic
