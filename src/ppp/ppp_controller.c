/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
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
#include "ppp/ppp.h"
#include "ethernet/ethernet.h"
#include "component.h"
#include "wan_manager_utils.h"

#define ME "ppp-ctrl"

static const char* ppp_get_client(UNUSED bool ipv4,
                                  UNUSED const char* intf_path,
                                  UNUSED const char* intf_alias) {
    SAH_TRACEZ_IN(ME);
    // For now a fixed path to the first instance is used, in the future it should be possible to get a specific instance
    SAH_TRACEZ_OUT(ME);
    return "Device.PPP.Interface.1.";
}

amxd_status_t ppp_enable(mode_ctrl_t mode,
                         const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    int ipmode = mode & (MASK_IPv4 | MASK_IPv6) & MASK_PPP;
    int ip_version = ((ipmode & MASK_IPv4) != 0) ? 4 : 6;
    amxd_status_t rc = amxd_status_unknown_error;
    const char* ppp_path = NULL;
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    const char* old_intf_path = GETP_CHAR(parameters, "old_interface_parameters.IPv6Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    const char* intf_path = ip_version == 4 ? GET_CHAR(parameters, "IPv4Reference") : GET_CHAR(parameters, "IPv6Reference");
    const char* username = GET_CHAR(parameters, "UserName");
    const char* password = GET_CHAR(parameters, "Password");
    const char* name = GET_CHAR(parameters, "Name");
    bool default_interface = GET_BOOL(parameters, "DefaultInterface");
    char* logical_path = NULL;
    const char* router_info = "Device.Routing.RouteInformation.";
    char* route_path = NULL;
    const char* nd_intf = "wan";

    SAH_TRACEZ_INFO(ME, "Enabling PPP%d", ip_version);
    when_str_empty_trace(intf_alias, exit, ERROR, "No IP interface alias found");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter for interface %s is empty", intf_path);

    // Get the matching PPP client
    ppp_path = ppp_get_client(true, intf_path, intf_alias);

    // VLANS
    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
        lower_layer = GET_CHAR(parameters, "VLANTermination");
    }

    // Set LowerLayer in PPP-manager
    rc = component_set_str_param(ppp_path, ppp_get_context(), "LowerLayers", lower_layer);
    when_failed(rc, exit);

    // Only override default PPP credentials if they are set in our Datamodel
    if((username != NULL) && (username[0] != 0)) {
        rc = component_set_str_param(ppp_path, ppp_get_context(), "Username", username);
        when_failed(rc, exit);
    }
    if((password != NULL) && (password[0] != 0)) {
        rc = component_set_str_param(ppp_path, ppp_get_context(), "Password", password);
        when_failed(rc, exit);
    }

    // Set LowerLayer in IP-manager
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", ppp_path);
    when_failed_trace(rc, exit, ERROR, "Failed to set '%s.LowerLayers' to '%s'", intf_path, ppp_path);

    if(ip_version == 4) {
        // Enable PPPv4
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPCPEnable", true);
        when_failed(rc, exit);

        // Enable the IPv4 Address instance
        rc = ipv4_addr_toggle(intf_path, NULL, PPP_ADDRESSING_TYPE, true);
        when_failed_trace(rc, exit, ERROR, "Failed to enable the IPv4 Address instance");

        // Enable IPv4 on the IP interface
        rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", true);
        when_failed_trace(rc, exit, ERROR, "Failed to enable IPv4 on %s", intf_path);
    } else {
        // Switch the parent prefix path to the correct IPv6Reference
        ip_parent_prefix_toggle(GET_CHAR(parameters, "DeferredIPv6Instances"), old_intf_path, intf_path);

        // Enable PPPv6
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPv6CPEnable", true);
        when_failed(rc, exit);

        // Enable IPv6 on the IP interface
        rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", true);
        when_failed_trace(rc, exit, ERROR, "Failed to enable IPv6 on %s", intf_path);
    }

    // Enable the PPP interface
    rc = component_set_enable(ppp_path, ppp_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable PPP instance '%s'", ppp_path);

    // Enable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

    // Set the default route origin
    if((ip_version == 4) && default_interface) {
        rc = routing_default_route_set_origin(intf_path, ROUTING_ORIGIN_IPCP, NULL);
        when_failed_trace(rc, exit, ERROR, "Failed to configure default IPv4 route");
    }

    if(ip_version == 6) {
        route_path = routing_get_interfacesetting(intf_path);
        rc = component_set_str_param(route_path, routing_get_context(), "Interface", intf_path);
        when_failed_trace(rc, exit, ERROR, "Failed to set the routing interface to %s'", intf_path);

        // Enable the RouteInformation instance
        rc = component_set_enable(router_info, routing_get_context(), true);
        when_failed_trace(rc, exit, ERROR, "Failed to enable the Routing manager's RoutingInformation");

        // Enable NeighborDiscovery for the wan
        nd_interface_setting_toggle(nd_intf, true);
    }

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv%dReference to '%s'", ip_version, logical_path);

    rc = amxd_status_ok;

exit:
    free(route_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t ppp_disable(mode_ctrl_t mode,
                          const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    int ipmode = mode & (MASK_IPv4 | MASK_IPv6) & MASK_PPP;
    int ip_version = ((ipmode & MASK_IPv4) != 0) ? 4 : 6;
    const char* nd_intf = "wan";

    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = ip_version == 4 ? GET_CHAR(parameters, "IPv4Reference") : GET_CHAR(parameters, "IPv6Reference");
    const char* ppp_path = ppp_get_client(true, intf_path, NULL);
    const char* router_info = "Device.Routing.RouteInformation.";
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;
    char* route_path = NULL;

    SAH_TRACEZ_INFO(ME, "Disabling PPP%d", ip_version);
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(ppp_path, exit, ERROR, "No PPP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    if(ip_version == 6) {
        // Disable NeighborDiscovery for the wan
        nd_interface_setting_toggle(nd_intf, false);
    }

    //Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv%dReference from '%s.LowerLayers'", ip_version, logical_path);

    // Disable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the whole IP interface %s", intf_path);

    // Disable the PPP interface
    rc = component_set_enable(ppp_path, ppp_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable PPP instance '%s'", ppp_path);

    if(ip_version == 4) {
        // Disable IPv4 on the IP interface
        rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", false);
        when_failed_trace(rc, exit, ERROR, "Failed to enable IPv4 on %s", intf_path);

        // Disable the IPv4 Address instance
        rc = ipv4_addr_toggle(intf_path, NULL, PPP_ADDRESSING_TYPE, false);
        when_failed(rc, exit);

        // Disable PPPv4
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPCPEnable", false);
        when_failed(rc, exit);
    } else {
        // Disable IPv6 on the IP interface
        rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", false);
        when_failed_trace(rc, exit, ERROR, "Failed to disable IPv6 on %s", intf_path);

        // Disable PPPv6
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPv6CPEnable", false);
        when_failed(rc, exit);
    }

    // Clear LowerLayer in IP-manager
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear '%s.LowerLayers'", intf_path);

    // Clear LowerLayer in PPP-manager
    rc = component_set_str_param(ppp_path, ppp_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear '%s.LowerLayers'", ppp_path);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

    if(ip_version == 6) {
        // Disable the RouteInformation instance
        rc = component_set_enable(router_info, routing_get_context(), false);
        when_failed_trace(rc, exit, ERROR, "Failed to disable the Routing manager's RoutingInformation");

        route_path = routing_get_interfacesetting(intf_path);
        rc = component_set_str_param(route_path, routing_get_context(), "Interface", "");
        when_failed_trace(rc, exit, ERROR, "Failed to remove the routing interface");
    }

exit:
    free(route_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}
