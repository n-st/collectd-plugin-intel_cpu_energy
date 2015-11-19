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

#include "rapl.h"

#include <unistd.h>

/*
 * The CPUs' energy usage should be checked regularly so we won't miss any
 * counter overflows. The plugin will therefore fall back to a maximum interval
 * if the global interval is too long.
 */
#define MAXIMUM_INTERVAL_MS 30000

uint64_t rapl_node_count = 0;
double **prev_sample = NULL;
double **cum_energy_J = NULL;

double get_rapl_energy_info (uint64_t power_domain, uint64_t node)
{
    int          err;
    double       total_energy_consumed;

    switch (power_domain) {
    case PKG:
        err = get_pkg_total_energy_consumed(node, &total_energy_consumed);
        break;
    case PP0:
        err = get_pp0_total_energy_consumed(node, &total_energy_consumed);
        break;
    case PP1:
        err = get_pp1_total_energy_consumed(node, &total_energy_consumed);
        break;
    case DRAM:
        err = get_dram_total_energy_consumed(node, &total_energy_consumed);
        break;
    default:
        err = MY_ERROR;
        break;
    }

    return total_energy_consumed;
}

static void energy_submit (unsigned int cpu_id, double measurements[4])
{
    value_t values[4];
    value_list_t vl = VALUE_LIST_INIT;

    values[0].gauge = measurements[0];
    values[1].gauge = measurements[1];
    values[2].gauge = measurements[2];
    values[3].gauge = measurements[3];

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
    int node;
    int domain;
    double new_sample;
    double delta;

    for (node = 0; node < rapl_node_count; node++) {
        for (domain = 0; domain < RAPL_NR_DOMAIN; ++domain) {
            if (is_supported_domain(domain)) {
                new_sample = get_rapl_energy_info(domain, node);
                delta = new_sample - prev_sample[node][domain];

                /* Handle wraparound */
                if (delta < 0) {
                    delta += MAX_ENERGY_STATUS_JOULES;
                }

                prev_sample[node][domain] = new_sample;
                cum_energy_J[node][domain] += delta;
            }
        }

        energy_submit(node, cum_energy_J[node]);
    }


    return (0);
}

static int energy_read_complex (user_data_t *user_data)
{
    return energy_read ();
}

static int energy_init (void)
{
    int node, domain;

    if (0 != init_rapl()) {
        terminate_rapl();
        return MY_ERROR;
    }
    rapl_node_count = get_num_rapl_nodes_pkg();

    prev_sample = malloc(rapl_node_count * sizeof(double*));
    cum_energy_J = malloc(rapl_node_count * sizeof(double*));

    /* Read initial values */
    for (node = 0; node < rapl_node_count; node++) {
        prev_sample[node] = malloc(RAPL_NR_DOMAIN * sizeof(double));
        cum_energy_J[node] = malloc(RAPL_NR_DOMAIN * sizeof(double));

        for (domain = 0; domain < RAPL_NR_DOMAIN; ++domain) {
            if (is_supported_domain(domain)) {
                prev_sample[node][domain] = get_rapl_energy_info(domain, node);
            }
        }
    }

    return 0;
}

static int energy_shutdown (void)
{
    terminate_rapl();

    return 0;
}

void module_register (void)
{
    plugin_register_init ("intel_cpu_energy", energy_init);
    plugin_register_shutdown ("intel_cpu_energy", energy_shutdown);

    uint64_t global_interval_ms = CDTIME_T_TO_MS(plugin_get_interval());
    if (global_interval_ms <= MAXIMUM_INTERVAL_MS)
    {
        /* use global interval */
        plugin_register_read ("intel_cpu_energy", energy_read);
    } else {
        /* override global interval with locally defined maximum interval */
        cdtime_t local_interval_cdtime = MS_TO_CDTIME_T(MAXIMUM_INTERVAL_MS);
        plugin_register_complex_read (/* group = */ NULL, "intel_cpu_energy",
                energy_read_complex, local_interval_cdtime, /* user data = */ NULL);
    }
} /* void module_register */
