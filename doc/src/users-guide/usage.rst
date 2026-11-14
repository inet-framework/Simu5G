Using Simu5G
============

Installation
------------

Simu5G requires OMNeT++ and a matching version of the INET Framework. The
versions a release was tested with are listed in its entry in
``WHATSNEW.md``; Simu5G releases are tied to specific INET versions.

The simplest way to install everything is ``opp_env``, which installs Simu5G
together with the OMNeT++ and INET versions it needs:

.. code:: shell

   $ opp_env init
   $ opp_env install simu5g-latest
   $ opp_env shell simu5g-latest

``simu5g-git`` instead of ``simu5g-latest`` installs the current development
version. Inside the shell opened by ``opp_env shell``, everything is built
and the environment is set up.

To build Simu5G by hand, against an OMNeT++ and an INET that are already
installed, source the ``setenv`` script of OMNeT++, of INET and of Simu5G (in
this order, each in its own root directory), and run ``make`` in the Simu5G
root directory:

.. code:: shell

   $ cd simu5g
   $ . setenv
   $ make                # release build
   $ make MODE=debug     # debug build

INET must have been built in the same mode. The debug build keeps the
model's internal consistency checks (``ASSERT``) enabled; use it when you
develop or extend the model. Simu5G can also be imported into the OMNeT++ IDE
as a project that references INET.

Some parts of Simu5G are optional project features, switched with
``opp_featuretool`` or in the IDE's project properties: D2D support
(``Simu5G_D2D``, enabled by default) and the vehicular examples with Veins
(``Simu5G_Cars``, disabled by default; they need Veins, its ``veins_inet``
subproject and SUMO).

The complete instructions, including the IDE and the feature details, are in
``INSTALL.md`` in the Simu5G root directory and on the
:doc:`Installation <../install>` page.

Directory layout
----------------

The parts of the Simu5G source tree relevant to a simulation user are:

``src/simu5g/``
   The model library: NED files, C++ sources and message definitions, in
   packages that follow the architecture -- ``nodes`` (the node types),
   ``stack`` (the cellular NIC and its layers, the radio and the radio
   medium), ``corenetwork`` (the core network's user plane and control
   plane), ``common`` (the Binder, the carriers, the BLER curves),
   ``x2``, ``apps``, ``background``, ``mobility`` and ``mec``. The NED
   Reference documents every module found here.

``simulations/``
   Ready-to-run example simulations, organized by technology (``lte/`` and
   ``nr/``) and by feature, plus ``channelmodel/`` for the channel models.
   The reference networks are in ``simulations/lte/networks/`` and
   ``simulations/nr/networks/`` (see :doc:`deployments`). Each example
   directory has an ``omnetpp.ini`` and usually a ``run`` script. The
   examples double as regression tests and as starting points for your own
   scenarios.

``emulation/``
   Examples for running Simu5G as a real-time network emulator (see
   :doc:`emulation`).

``showcases/``, ``tutorials/``
   The simulations behind the showcases and tutorials of the Simu5G website.

``tests/``
   The automated tests: fingerprint tests of the example simulations, unit
   tests and module tests.

``doc/src/``
   The sources of the Simu5G website, including this manual.

Running example simulations
---------------------------

Example simulations can be run from the OMNeT++ IDE (select the example's
folder or its ``omnetpp.ini``, and click *Run*), or from the command line. On
the command line, first source the ``setenv`` script in the Simu5G root
directory: it sets ``SIMU5G_ROOT`` and puts Simu5G's ``bin`` directory on the
``PATH``. Then run the simulation from its directory, either with its ``run``
script or with the ``simu5g`` launcher:

.. code:: shell

   $ cd simulations/nr/tutorial
   $ ./run
   $ simu5g -c Single-UE omnetpp.ini

The ``simu5g`` launcher loads the Simu5G library and sets up the NED path for
Simu5G and INET, so use it rather than ``opp_run``. It needs ``SIMU5G_ROOT``
and ``INET_ROOT`` to be set. ``simu5g_dbg`` runs the debug build in the same
way.

Without other options, the simulation starts in Qtenv, the graphical runtime
environment. It is recommended for getting acquainted with the models: it
lets you inspect the network, watch packets flow through the protocol stack,
and browse module parameters and statistics interactively. Batch runs use the
command-line environment, Cmdenv, with ``-u Cmdenv``:

.. code:: shell

   $ simu5g -u Cmdenv -c Single-UE omnetpp.ini

Configuring simulations
-----------------------

Simulations are configured the standard OMNeT++ way:

*  The *network* -- which nodes exist and how they are wired -- is described
   in NED files. :doc:`deployments` describes how cellular networks are
   assembled, and the reference networks can be used as they are.

*  Everything else -- module types for open slots, parameter values, traffic,
   mobility, bearers, channel model settings -- is given in the INI file.

Because nearly every aspect of the cellular stack is a module parameter,
typical research tasks (trying a different scheduling policy, changing the
numerology, adding carriers, switching channel models) require only INI file
changes. The core of a configuration for a 5G standalone network with one UE
looks like this (from ``simulations/nr/tutorial/omnetpp.ini``):

.. code:: ini

   [General]
   network = simu5g.simulations.nr.networks.SingleCell_Standalone

   [Config Single-UE]
   # gNodeB position
   *.gnb.mobility.initialX = 500m
   *.gnb.mobility.initialY = 500m

   # one UE, 400 m from the gNodeB
   *.numUe = 1
   *.ue[*].mobility.initialX = 500m
   *.ue[*].mobility.initialY = 900m

   # attach the UE's NR leg to the gNodeB
   *.ue[*].servingNodeId = 0
   *.ue[*].nrServingNodeId = 1

:doc:`building-a-scenario` builds a complete configuration around these lines,
step by step.

Collecting and analyzing results
--------------------------------

Simu5G modules record statistics through OMNeT++'s standard signal and
statistic mechanism, so everything the OMNeT++ documentation says about
result recording, filtering and analysis applies. :doc:`results` describes the
statistics of the cellular stack and how to analyze them.

Running the tests
-----------------

The tests are run from the Simu5G root directory: ``make tests`` runs the
fingerprint tests, ``make unittests`` the unit tests, and ``make
moduletests`` the module tests. Add ``MODE=debug`` to run the unit and module
tests against the debug build. The ``README`` files under ``tests/`` describe
each kind.

Getting help
------------

*  The NED Reference is the module-level reference.

*  The example simulations show working configurations for every major
   feature.

*  Questions can be asked on the OMNeT++ users forum, and bugs reported as
   GitHub issues; see the :doc:`Contacts <../contacts>` page.
