Using Simu5G
============

Installation
------------

Simu5G requires a working OMNeT++ installation and a compatible version of the
INET Framework. The supported installation methods -- installing from the
OMNeT++ IDE, building from source, and using ``opp_env`` -- are described on
the :doc:`Installation <../install>` page and are not repeated here.

.. TODO: once the manual is built as a standalone book (PDF), inline a short
   version of the installation instructions here.

Directory layout
----------------

The parts of the Simu5G source tree relevant to a simulation user are:

``src/``
   The model library itself: NED files, C++ sources, and message definitions.
   The generated NED documentation is the reference for every module found
   here.

``simulations/``
   Ready-to-run example simulations, organized by technology (``lte/`` and
   ``nr/``) and by feature. Each directory contains a network description
   (NED file), an ``omnetpp.ini`` configuration, and often a ``run`` script.
   These examples double as regression tests and as starting points for your
   own scenarios.

``emulation/``
   Examples for running Simu5G as a real-time network emulator (see
   :doc:`emulation`).

``tests/``
   Automated tests, including the fingerprint regression test suite.

Running example simulations
---------------------------

Example simulations can be run from the OMNeT++ IDE (right-click the INI
file, then *Run As* or *Debug As*), or from the command line. On the command
line, first source the ``setenv`` script in the Simu5G root directory --
this puts the ``simu5g`` launcher on the PATH -- then run the simulation
from its directory (most example directories also provide a ``run`` script
that does the same):

.. code:: shell

   $ cd simulations/nr/standalone
   $ simu5g

The graphical runtime environment (Qtenv) is the default and is recommended
for getting acquainted with the models: it lets you inspect the network
graphically, watch packets flow through the protocol stack, and browse module
parameters and statistics interactively. For batch experiments, use the
command-line environment (Cmdenv) by adding ``-u Cmdenv``.

Configuring simulations
-----------------------

Simulations are configured the standard OMNeT++ way:

*  The *network* -- which nodes exist and how they are wired -- is described
   in NED files. Chapter :doc:`networks` describes how cellular networks
   are assembled.
*  Everything else -- module types to use for open slots, parameter values,
   traffic, mobility, channel model settings -- is given in the INI file.

Because nearly every aspect of the cellular stack is a module parameter,
typical Simu5G research tasks (trying a different scheduling policy, changing
the numerology, adding carriers, switching channel models) require only INI
file changes. The chapters of this manual point out the relevant parameters
in each area; the complete list is in the NED documentation.

The essential elements of an INI configuration for a 5G standalone network
look like this (taken from ``simulations/nr/standalone/omnetpp.ini``):

.. code:: ini

   [General]
   network = simu5g.simulations.nr.networks.SingleCell_Standalone

   # number of resource blocks per carrier
   **.numBands = 50

   # connect the UE's NIC to the corresponding serving gNB (NR side only)
   *.ue[0].servingNodeId = 0
   *.ue[0].nrServingNodeId = 1

   # positions (gNB fixed, UE placed via its mobility module)
   *.gnb.mobility.initialX = 450m
   *.gnb.mobility.initialY = 300m

   # traffic: any INET application works
   *.ue[0].numApps = 1
   *.ue[0].app[0].typename = "UdpBasicApp"
   ...

Collecting and analyzing results
--------------------------------

Simu5G modules record statistics through OMNeT++'s standard signal/statistic
mechanism, so everything from the OMNeT++ documentation about result
recording, filtering and analysis applies. Chapter :doc:`results` lists
the most important statistics recorded by the cellular stack.

Getting help
------------

*  The generated NED documentation is the module-level reference.
*  The example simulations show working configurations for every major
   feature.
*  Questions can be asked on GitHub, on the Simu5G mailing list, or on the
   OMNeT++ forums; see the Contacts page of the Simu5G website.
