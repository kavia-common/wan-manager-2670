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

#define DSLITE_PATH "DSLite."
#define DSLITE_IF_PATH "DSLite.InterfaceSetting.1"
#define LOGICAL_PATH "Logical.Interface.1."

amxd_status_t dslite_enable(UNUSED mode_ctrl_t mode,
                            const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* ipv4_path = GET_CHAR(parameters, "IPv4Reference");
    const char* name = GET_CHAR(parameters, "Name");
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    char* dhcpv6_path = NULL;
    char* logical_path = NULL;
    amxc_var_t wan_if;
    bool default_interface = GET_BOOL(parameters, "DefaultInterface");
    amxc_var_t* ipv4 = GET_ARG(parameters, "ipv4");

    amxc_var_init(&wan_if);

    when_str_empty_trace(ipv4_path, exit, ERROR, "No IPv4 interface path found");

    //Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", ipv4_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv6Reference to '%s'", logical_path);

    // Setting up the default route instance for static ipv4
    if(default_interface) {
        rc = routing_default_route_set_origin(ipv4_path, ROUTING_ORIGIN_STATIC, GET_CHAR(ipv4, "DefaultRouter"));
        when_failed_trace(rc, exit, ERROR, "Failed to configure default IPv4 route");
    }

    // Enable DSLite
    rc = component_set_enable(DSLITE_PATH, dslite_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable DSLite instance '%s'", DSLITE_PATH);

    // Enable PCP
    rc = component_set_bool("PCP.", pcp_get_context(), "Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable PCP");

    // Enable DHCPv6 Client
    component_get_param(&wan_if, DSLITE_IF_PATH, dslite_get_context(), "WANInterface");
    when_str_empty_trace(GETP_CHAR(&wan_if, "0.0.WANInterface"), exit, ERROR, "Failed to get DSLite WANInterface parameter");

    dhcpv6_path = dhcpc_get_client(false, GETP_CHAR(&wan_if, "0.0.WANInterface"), intf_alias);

    when_str_empty_trace(dhcpv6_path, exit, ERROR, "Failed to get DHCPv6 client instance path");

    component_set_enable(dhcpv6_path, dhcpv6_get_context(), true);

exit:
    amxc_var_clean(&wan_if);
    free(dhcpv6_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dslite_disable(UNUSED mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* ipv4_path = GET_CHAR(parameters, "IPv4Reference");
    const char* name = GET_CHAR(parameters, "Name");
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    char* logical_path = NULL;
    char* dhcpv6_path = NULL;
    amxc_var_t wan_if;

    amxc_var_init(&wan_if);

    // Disable DHCPv6 Client
    component_get_param(&wan_if, DSLITE_IF_PATH, dslite_get_context(), "WANInterface");
    when_str_empty_trace(GETP_CHAR(&wan_if, "0.0.WANInterface"), exit, ERROR, "Failed to get DSLite WANInterface parameter");

    dhcpv6_path = dhcpc_get_client(false, GETP_CHAR(&wan_if, "0.0.WANInterface"), intf_alias);

    when_str_empty_trace(dhcpv6_path, exit, ERROR, "Failed to get DHCPv6 client instance path");

    component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);

    //Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", ipv4_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv4Reference from '%s'", logical_path);

    // Disable DSLite
    rc = component_set_enable(DSLITE_PATH, dslite_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable DSLite instance '%s'", DSLITE_PATH);

    // Disable PCP
    rc = component_set_bool("PCP.", pcp_get_context(), "Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable PCP");

exit:
    amxc_var_clean(&wan_if);
    free(dhcpv6_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}