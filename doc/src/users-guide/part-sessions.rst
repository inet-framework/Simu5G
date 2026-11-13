Sessions and QoS
================

A UE reaches a data network through a PDU session, and the traffic of a session
is carried in QoS flows that the radio access network maps to data radio
bearers. This part describes how Simu5G models sessions and QoS, following the
5G QoS model of 3GPP TS 23.501, the SDAP layer of TS 37.324, and, for LTE, the
EPS bearer model of TS 23.401.

PDU Sessions describes the session model and the IP and Unstructured session
types. Ethernet Sessions covers the Ethernet session type, which turns the
5G system into an Ethernet bridge. QoS Flows and Bearers describes how traffic
is classified into QoS flows, how bearers are defined and configured, and how
their RAN-side settings follow from their QoS characteristics.

.. toctree::
   :maxdepth: 1

   pdu-sessions
   ethernet-sessions
   qos
