Radio
=====

This part describes the radio environment of a Simu5G network: the spectrum the
network uses and the medium that carries the transmissions. Spectrum covers
component carriers, numerologies and TDD (3GPP TS 38.211 and TS 38.213).
The Radio Medium and Channel Models covers the network-level module that
models propagation, interference and the SINR, and the channel models it
evaluates, among them the models of 3GPP TR 38.901 and TR 36.873.

In Simu5G, as in INET, the radio medium models everything up to the receiving
antenna, and the node decides what to make of a reception. The node's side --
error models, HARQ and CQI feedback -- is described in the Radio Protocol
Stack part.

.. toctree::
   :maxdepth: 1

   spectrum
   radio-medium
