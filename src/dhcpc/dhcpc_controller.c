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
#include <stdio.h>
#include <stdlib.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxc/amxc_macros.h>

#include "ctrl/mode_ctrl.h"
#include "dhcpc/dhcpc.h"
#include "ethernet/ethernet.h"
#include "component.h"
#include "wan_manager_utils.h"

#define ME "dhcpc-ctrl"

static amxd_status_t dhcpc4_enable(UNUSED mode_ctrl_t mode,
                                   const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* dhcpv4_path = GET_CHAR(parameters, "DHCPv4Reference");
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");

    SAH_TRACEZ_INFO(ME, "Enabling DHCPv4");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");

    // Enable the DHCPv4 Client
    when_str_empty_trace(dhcpv4_path, exit, ERROR, "No DHCPv4 client found with Interface='%s'", intf_path);
    rc = component_set_str_param(dhcpv4_path, dhcpv4_get_context(), "Interface", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to set DHCPv4Reference Interface to '%s'", intf_path);
    rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), true);

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t dhcpc4_disable(UNUSED mode_ctrl_t mode,
                                    const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* dhcpv4_path = GET_CHAR(parameters, "DHCPv4Reference");

    SAH_TRACEZ_INFO(ME, "Disabling DHCPv4");
    when_str_empty(intf_path, exit);

    // Disable the DHCPv4 Client
    if(dhcpv4_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
        rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), false);
        when_failed_trace(rc, exit, ERROR, "Failed to disable '%s'", dhcpv4_path);
        rc = component_set_str_param(dhcpv4_path, dhcpv4_get_context(), "Interface", "");
        when_failed_trace(rc, exit, ERROR, "Failed to clear DHCPv4Reference Interface");
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv4 client found with Interface='%s'", intf_path);
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dhcp4_layer(mode_ctrl_t mode,
                          amxc_var_t* const parameters,
                          bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = dhcpc4_enable(mode, parameters);
    } else {
        rc = dhcpc4_disable(mode, parameters);
    }

    return rc;
}

static amxd_status_t dhcpc6_enable(UNUSED mode_ctrl_t mode,
                                   const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* dhcpv6_path = GET_CHAR(parameters, "DHCPv6Reference");
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    char* route_path = routing_get_interfacesetting(intf_path);
    const char* name = GET_CHAR(parameters, "Name");
    const char* router_info = DEVICE_PATH "Routing.RouteInformation.";

    SAH_TRACEZ_INFO(ME, "Enabling DHCPv6");
    when_str_empty(intf_alias, exit);
    when_str_empty(intf_path, exit);
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    // Fill the interface reference of the RouteInformation in
    rc = component_set_str_param(route_path, routing_get_context(), "Interface", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to update the Routing manager's interface with %s", intf_path);

    // Enable the RouteInformation instance
    rc = component_set_enable(router_info, routing_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the Routing manager's RoutingInformation");

    // Enable the DHCPv6 Client
    if(dhcpv6_path != NULL) {
        rc = component_set_str_param(dhcpv6_path, dhcpv6_get_context(), "Interface", intf_path);
        when_failed_trace(rc, exit, ERROR, "Failed to set DHCPv6Reference Interface to '%s'", intf_path);
        rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), true);
        when_failed_trace(rc, exit, ERROR, "Failed to enable DHCPv6 Client for Interface '%s'", intf_path);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv6 client found with Interface='%s'", intf_path);
    }

    // Enable NeighborDiscovery for the wan
    rc = nd_interface_setting_toggle(NEIGH_DISCOVERY_INTF, "Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable NeighborDiscovery");

exit:
    free(route_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t dhcpc6_disable(UNUSED mode_ctrl_t mode,
                                    const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    const char* dhcpv6_path = GET_CHAR(parameters, "DHCPv6Reference");
    char* route_path = routing_get_interfacesetting(intf_path);
    const char* router_info = DEVICE_PATH "Routing.RouteInformation.";
    const char* name = GET_CHAR(parameters, "Name");

    SAH_TRACEZ_INFO(ME, "Disabling DHCPv6");
    when_str_empty_trace(intf_path, exit, ERROR, "Interface path is empty");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    // Disable NeighborDiscovery for the wan
    rc = nd_interface_setting_toggle(NEIGH_DISCOVERY_INTF, "Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable NeighborDiscovery");

    // Disable the DHCPv6 Client
    if(dhcpv6_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv6 path for %s -> %s", intf_path, dhcpv6_path);
        rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);
        when_failed(rc, exit);
        rc = component_set_str_param(dhcpv6_path, dhcpv6_get_context(), "Interface", "");
        when_failed_trace(rc, exit, ERROR, "Failed to clear DHCPv6Reference Interface");
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv6 client found with Interface='%s'", intf_path);
    }

    // Disable the RouteInformation instance
    rc = component_set_enable(router_info, routing_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the Routing manager's RoutingInformation");

    // Empty the interface reference of the RouteInformation
    rc = component_set_str_param(route_path, routing_get_context(), "Interface", "");
    when_failed_trace(rc, exit, ERROR, "Failed to empty the Routing manager's interface");

exit:
    free(route_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dhcp6_layer(UNUSED mode_ctrl_t mode,
                          amxc_var_t* const parameters,
                          bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = dhcpc6_enable(mode, parameters);
    } else {
        rc = dhcpc6_disable(mode, parameters);
    }

    return rc;
}
