/**
 * collectd - intel_cpu_energy.c
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
 * Author:
 *   Nils Steinger <git at n-st dot de>
 * Based on the load plugin developed by:
 *   Florian octo Forster <octo at collectd.org>
 *   Manuel Sanmartin
 *   Vedran Bartonicek <vbartoni at gmail.com>
 **/

#if ! HAVE_CONFIG_H

#include <stdlib.h>

#include <string.h>

#ifndef __USE_ISOC99 /* required for NAN */
# define DISABLE_ISOC99 1
# define __USE_ISOC99 1
#endif /* !defined(__USE_ISOC99) */
#include <math.h>
#if DISABLE_ISOC99
# undef DISABLE_ISOC99
# undef __USE_ISOC99
#endif /* DISABLE_ISOC99 */

#include <time.h>

#endif /* ! HAVE_CONFIG */

#ifdef COLLECTD_VERSION_LT_5_5
# include <core/collectd.h>
# include <core/common.h>
# include <core/plugin.h>
#else
/*
 * collectd daemon files were moved to the daemon/ subdirectory in commit
 * 216c6246b73645ac093de15b87aedc9abc6ebc80 (2014-09-20).
 */
# include <core/daemon/collectd.h>
# include <core/daemon/common.h>
# include <core/daemon/plugin.h>
#endif /* COLLECTD_VERSION_LT_5_5 */

#include "rapl.h"

#include <unistd.h>

/*
 * The CPUs' energy usage should be checked regularly so we won't miss any
 * counter overflows. The plugin will therefore fall back to a maximum interval
 * if the global interval is too long.
 */
#define MAXIMUM_INTERVAL_MS 60000

static const char * const RAPL_DOMAIN_NAMES[RAPL_NR_DOMAIN] = {
    "package",
    "core",
    "uncore",
    "dram"
};

uint64_t rapl_node_count = 0;
double **prev_sample = NULL;
double **cum_energy_J = NULL;
// Not to be confused with is_supported_domain()!
double **rapl_domain_actually_supported = NULL;

int get_rapl_energy_info (uint64_t power_domain, uint64_t node, double *total_energy_consumed)
{
    int          err;

    switch (power_domain) {
    case PKG:
        err = get_pkg_total_energy_consumed(node, total_energy_consumed);
        break;
    case PP0:
        err = get_pp0_total_energy_consumed(node, total_energy_consumed);
        break;
    case PP1:
        err = get_pp1_total_energy_consumed(node, total_energy_consumed);
        break;
    case DRAM:
        err = get_dram_total_energy_consumed(node, total_energy_consumed);
        break;
    default:
        err = MY_ERROR;
        break;
    }

    return err;
}

static int energy_submit (unsigned int cpu_id, unsigned int domain, double measurement)
{
    /*
     * An Identifier is of the form host/plugin-instance/type-instance with
     * both instance-parts being optional.
     * In our case: [host]/intel_cpu_energy-[e.g. cpu0]/energy-[e.g. package]
     */

    value_list_t vl = VALUE_LIST_INIT;

    value_t values[1];
    values[0].gauge = measurement;
    vl.values = values;
    vl.values_len = STATIC_ARRAY_SIZE (values);

    sstrncpy (vl.host, hostname_g, sizeof (vl.host));
    sstrncpy (vl.plugin, "intel_cpu_energy", sizeof (vl.plugin));
    ssnprintf (vl.plugin_instance, sizeof (vl.plugin_instance), "cpu%u", cpu_id);
    sstrncpy (vl.type, "energy", sizeof (vl.type));
    sstrncpy (vl.type_instance, RAPL_DOMAIN_NAMES[domain], sizeof (vl.type_instance));

    return plugin_dispatch_values (&vl);
}

static int energy_read (void)
{
    int err;
    int node;
    int domain;
    double new_sample;
    double delta;

    for (node = 0; node < rapl_node_count; node++) {
        for (domain = 0; domain < RAPL_NR_DOMAIN; ++domain) {
            if (rapl_domain_actually_supported[node][domain]) {
                err = get_rapl_energy_info(domain, node, &new_sample);
                if (err) {
                    ERROR ("intel_cpu_energy plugin: Failed to get RAPL energy information for node %d, domain %d (%s): Return value %d", node, domain, RAPL_DOMAIN_NAMES[domain], err);
                    return err;
                }

                delta = new_sample - prev_sample[node][domain];

                /* Handle wraparound */
                if (delta < 0) {
                    delta += MAX_ENERGY_STATUS_JOULES;
                }

                prev_sample[node][domain] = new_sample;
                cum_energy_J[node][domain] += delta;

                err = energy_submit(node, domain, cum_energy_J[node][domain]);
                if (err) {
                    ERROR ("intel_cpu_energy plugin: Failed to submit energy information for node %d, domain %d (%s): Return value %d", node, domain, RAPL_DOMAIN_NAMES[domain], err);
                    return err;
                }
            }
        }
    }


    return (0);
}

static int energy_read_complex (user_data_t *user_data)
{
    return energy_read ();
}

static int energy_init (void)
{
    int err, node, domain;

    err = init_rapl();
    if (0 != err) {
        ERROR ("intel_cpu_energy plugin: RAPL initialisation failed with return value %d", err);
        terminate_rapl();
        return MY_ERROR;
    }
    rapl_node_count = get_num_rapl_nodes_pkg();
    INFO ("intel_cpu_energy plugin: found %lu nodes (physical CPUs)", rapl_node_count);

    prev_sample = calloc(rapl_node_count, sizeof(double*));
    cum_energy_J = calloc(rapl_node_count, sizeof(double*));
    rapl_domain_actually_supported = calloc(rapl_node_count, sizeof(double*));
    if (prev_sample == NULL || cum_energy_J == NULL) {
        ERROR ("intel_cpu_energy plugin: Memory allocation failed for outer persistent array");
        return MY_ERROR;
    }

    /* Read initial values */
    for (node = 0; node < rapl_node_count; node++) {
        prev_sample[node] = calloc(RAPL_NR_DOMAIN, sizeof(double));
        cum_energy_J[node] = calloc(RAPL_NR_DOMAIN, sizeof(double));
        rapl_domain_actually_supported[node] = calloc(RAPL_NR_DOMAIN, sizeof(double));
        if (prev_sample[node] == NULL || cum_energy_J[node] == NULL) {
            ERROR ("intel_cpu_energy plugin: Memory allocation failed for inner persistent array (node %d)", node);
            return MY_ERROR;
        }

        for (domain = 0; domain < RAPL_NR_DOMAIN; ++domain) {
            rapl_domain_actually_supported[node][domain] = 0;

            if (is_supported_domain(domain)) {
                DEBUG ("intel_cpu_energy plugin: Node %d claims it supports domain %d (%s)", node, domain, RAPL_DOMAIN_NAMES[domain]);
                rapl_domain_actually_supported[node][domain] = 1;

                if (0 != get_rapl_energy_info(domain, node, &(prev_sample[node][domain]))) {
                    WARNING ("intel_cpu_energy plugin: Node %d claims it supports domain %d (%s) but an attempt to read it"
                             "has failed with return value %d. Will not try to read this domain of this node again.",
                             node, domain, RAPL_DOMAIN_NAMES[domain], err);
                    rapl_domain_actually_supported[node][domain] = 0;
                }
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

#ifdef COLLECTD_VERSION_LT_5_5
        /*
         * As of commit cce136946b879557f91183e4de58e92b81e138c8 (2015-06-06),
         * plugin_register_complex_read expects the interval to be of type
         * cdtime_t. Prior to that, it used to be a struct timespec.
         * Uncomment the appropriate line for the API version you're using.
         */

        /* Anything up to, and including, version 5.5.0: */
        struct timespec interval;
        interval.tv_sec = MAXIMUM_INTERVAL_MS / 1000;
        interval.tv_nsec = (MAXIMUM_INTERVAL_MS % 1000) * 1000L;

#else
        /*
         * Anything that includes the commit cce1369, e.g. the master branch,
         * but apparently not the releases 5.5.0 and 5.5.1.
         */
        cdtime_t interval = MS_TO_CDTIME_T(MAXIMUM_INTERVAL_MS);

#endif /* COLLECTD_VERSION_LT_5_5 */

        plugin_register_complex_read (/* group = */ NULL, "intel_cpu_energy",
                energy_read_complex, &interval, /* user data = */ NULL);
    }
} /* void module_register */
