Background Traffic and Interference
===================================

.. TODO: This chapter covers: BackgroundCell, ExtCell and the background traffic generators.

Background traffic
------------------

For load that should consume radio resources without being simulated
end-to-end, do not use application modules at all: the background traffic
generator machinery (background UEs inside real or background cells) is
far cheaper and designed exactly for that. It is described in
:doc:`emulation`.

Interference-only elements
--------------------------

Two node types create realistic load and interference without the cost of
simulating full protocol stacks:

*  ``BackgroundCell`` -- a lightweight cell with its own scheduler,
   channel model and a configurable population of background UEs (see
   :doc:`emulation`, where the background traffic model is described in
   detail);
*  ``ExtCell`` -- a fictitious neighboring cell that occupies a
   configurable part of the spectrum with a given transmission power and
   direction.

Both can be dropped into any network and configured from the INI file.

Nodes for modeling cell load and interference
---------------------------------------------

``BackgroundCell`` is a lightweight stand-in for a base station: it
contains a background scheduler, a dedicated channel model, and a
configurable population of background UEs, but no real protocol stack, so it
generates realistic interference at a small fraction of the cost of a full
cell. ``ExtCell`` is an even simpler, fictitious neighboring cell that
occupies configured resource blocks with a given transmission power. Both
are covered in more detail in :doc:`emulation`, together with the
background traffic generator that can also be embedded into *real* base
stations.
