AUTOMAKE_OPTIONS = foreign no-dependencies

pkginclude_HEADERS = #collectd/client.h collectd/network.h collectd/network_buffer.h collectd/lcc_features.h
lib_LTLIBRARIES = lib_intel_cpu_energy.la
nodist_pkgconfig_DATA = #lib_intel_cpu_energy.pc

#BUILT_SOURCES = collectd/lcc_features.h


lib_intel_cpu_energy_la_SOURCES = cpuid.c msr.c rapl.c
lib_intel_cpu_energy_la_CPPFLAGS = $(AM_CPPFLAGS) \
				-I$(top_srcdir)/intel_cpu_energy
lib_intel_cpu_energy_la_LDFLAGS = -version-info 1:0:0
lib_intel_cpu_energy_la_LIBADD = 
