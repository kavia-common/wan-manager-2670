/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include <string.h>
#include <stdlib.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxc/amxc_macros.h>

#include "ctrl/mode_ctrl.h"
#include "dhcpc/dhcpc.h"
#include "dslite/dslite.h"
#include "ethernet/ethernet.h"
#include "component.h"
#include "wan_manager_utils.h"
#include "dm_wan-manager.h"

#define ME "dslite-ctrl"

#define DSLITE_DEVICE_PATH DEVICE_PATH DSLITE_PATH
#define PCP_DEVICE_PATH DEVICE_PATH PCP_PATH

static amxd_status_t dslite_enable(UNUSED mode_ctrl_t mode,
                                   amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* ipv4_path = NULL;
    const char* dhcpv6_path = NULL;
    const char* dslite_path = NULL;
    const char* pcp_path = NULL;
    const char* dslite_wan_if = NULL;
    amxc_var_t wan_if;

    amxc_var_init(&wan_if);
    ipv4_path = get_ip_path(parameters, IPv4);
    when_str_empty_trace(ipv4_path, exit, ERROR, "No IPv4 interface path found");

    dhcpv6_path = get_dhcp_path(parameters, IPv6);
    when_str_empty_trace(dhcpv6_path, exit, ERROR, "Failed to get DHCPv6 client instance path");

    dslite_path = get_dslite_path(parameters);
    when_str_empty_trace(dslite_path, exit, ERROR, "Failed to get DSLite interface path");

    pcp_path = get_pcp_path(parameters);
    when_str_empty_trace(pcp_path, exit, ERROR, "Failed to get PCP client path");

    // Enable DSLite
    rc = component_set_enable(DSLITE_DEVICE_PATH, dslite_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable DSLite");
    rc = component_set_enable(dslite_path, dslite_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable DSLite Interface '%s'", dslite_path);

    // Enable PCP
    rc = component_set_enable(PCP_DEVICE_PATH, pcp_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable PCP");
    rc = component_set_enable(pcp_path, pcp_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable PCP Client %s", pcp_path);

    component_get_param(&wan_if, dslite_path, dslite_get_context(), "WANInterface");
    dslite_wan_if = GETP_CHAR(&wan_if, "0.0.WANInterface");
    when_str_empty_trace(dslite_wan_if, exit, ERROR, "Failed to get DSLite WANInterface parameter");

    rc = component_set_str_param(dhcpv6_path, dhcpv6_get_context(), "Interface", dslite_wan_if);
    when_failed_trace(rc, exit, ERROR, "Failed to set " DHCPV6_REFERENCE_PATH " Interface to '%s'", dslite_wan_if);
    component_set_enable(dhcpv6_path, dhcpv6_get_context(), true);

exit:
    amxc_var_clean(&wan_if);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t dslite_disable(UNUSED mode_ctrl_t mode,
                                    amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* dhcpv6_path = get_dhcp_path(parameters, IPv6);
    const char* dslite_path = NULL;
    const char* pcp_path = NULL;

    dslite_path = get_dslite_path(parameters);
    when_str_empty_trace(dslite_path, exit, ERROR, "Failed to get DSLite interface path");
    pcp_path = get_pcp_path(parameters);

    // Disable DHCPv6 Client
    when_str_empty_trace(dhcpv6_path, exit, ERROR, "Failed to get DHCPv6 client instance path");
    component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);
    rc = component_set_str_param(dhcpv6_path, dhcpv6_get_context(), "Interface", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear " DHCPV6_REFERENCE_PATH " Interface");

    // Disable DSLite
    rc = component_set_enable(dslite_path, dslite_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable DSLite instance '%s'", dslite_path);

    // Disable PCP
    rc = component_set_bool(pcp_path, pcp_get_context(), "Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable PCP");

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dslite_layer(mode_ctrl_t mode,
                           amxc_var_t* const parameters,
                           bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    if(enable) {
        rc = dslite_enable(mode, parameters);
    } else {
        rc = dslite_disable(mode, parameters);
    }

    return rc;
}

