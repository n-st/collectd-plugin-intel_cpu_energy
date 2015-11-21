Collectd plugin "intel_cpu_energy"
==================================

This [collectd][collectd] plugin measures and reports the power usage of 2nd
Generation (or later) Intel® Core™ processors.
It will report up to four values for each physical processor: the accumulated
energy usage (in Joules) for the "package", "core", "uncore", and "dram"
domains, as reported by the CPU. Not all domains are supported by all CPU
models, so some of them might be missing on your system.
The code for these measurements is based on Intel's [Power Gadget 2.5 for
Linux][powergadget].

Requirements
------------

To read the CPU's internal energy measurements, the plugin requires write
access to the CPU's model-specific registers (MSRs). For this, **the `msr`
kernel module has to be loaded**.
In addition, write access to /dev/cpu/\*/msr, along with the SYS_RAWIO
capability, is required, but since collectd runs its plugins with root
privileges anyway, those permissions should be available by default.

To build the module, you will need to have the `collectd-dev` package installed
(the package name might differ on non-Debian-based distributions).

The module has been successfully tested with version 5.4.0-3ubuntu2 of
`collectd-dev` (on Ubuntu 14.04), but collectd's plugin API can be highly
unstable, even between minor versions, so it might not work out of the box with
other versions.

Building and installing
-----------------------

    make clean all
    cp intel_cpu_energy.so /usr/lib/collectd/
    cp energy-type.db /etc/collectd/
    echo 'LoadPlugin intel_cpu_energy' > /etc/collectd/collectd.conf.d/intel_cpu_energy.conf
    echo 'TypesDB "/usr/share/collectd/types.db" "/etc/collectd/energy-type.db"' >> /etc/collectd/collectd.conf.d/intel_cpu_energy.conf
    service collectd restart

You might need to change the `TypesDB` line if you are already using a custom
data-set definition file (e.g. `my_types.db`).

Usage
-----

The plugin will produce data sets named according to the pattern

    ${host}/intel_cpu_energy-cpu{0..}/energy-{package,core,uncore,dram}

Each data point will contain the accumulated energy consumption in Joules (=
Watt seconds) since the last (re-)start of `collectd`.
The value is internally processed as a double-precision floating point number,
so you shouldn't encounter any problems with overflows.


[collectd]: https://github.com/collectd/collectd/
[powergadget]: https://software.intel.com/en-us/articles/intel-power-gadget-20
