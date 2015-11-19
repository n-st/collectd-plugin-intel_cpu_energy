/**
 * collectd - src/intel_cpu_energy.c
 * Copyright (C) 2015  Nils Steinger
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Nils Steinger <nst at voidptr.com>
 * Based on the load plugin developed by:
 *   Florian octo Forster <octo at collectd.org>
 *   Manuel Sanmartin
 *   Vedran Bartonicek <vbartoni at gmail.com>
 **/

#include "collectd.h"
#include "common.h"
#include "plugin.h"

#include <unistd.h>

static void energy_submit (unsigned int cpu_id, gauge_t package, gauge_t core, gauge_t uncore, gauge_t dram)
{
    value_t values[4];
    value_list_t vl = VALUE_LIST_INIT;

    values[0].gauge = package;
    values[1].gauge = core;
    values[2].gauge = uncore;
    values[3].gauge = dram;

    vl.values = values;
    vl.values_len = STATIC_ARRAY_SIZE (values);

    sstrncpy (vl.host, hostname_g, sizeof (vl.host));
    sstrncpy (vl.plugin, "intel_cpu_energy", sizeof (vl.plugin));
	ssnprintf (vl.plugin_instance, sizeof (vl.plugin_instance), "cpu%u", cpu_id);
    sstrncpy (vl.type, "intel_cpu_energy", sizeof (vl.type));

    plugin_dispatch_values (&vl);
}

static int energy_read (void)
{
    energy_submit(99, 41, 42, 43, 44);

    return (0);
}

void module_register (void)
{
    plugin_register_read ("intel_cpu_energy", energy_read);
} /* void module_register */
