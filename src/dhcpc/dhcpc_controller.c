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

/**
 * @brief
 *
 *  Function that outputs the path of the corresponding client regarding the interface input.
 *  If the function does not find the client with the right interface, it takes the first upcoming instance
 *  and assigns the interface to it before outputting the corresponding path.
 *
 * @param ipv4 IPv4 if set to true, IPv6 if set to false`
 * @param intf_path Interface path to the client from the IP-manager
 * @param intf_alias Alias of the related IP-manager interface to the DHCP client
 * @return char pointer containing the path to the client with the corresponding interface. Needs to be freed when no longer needed.
 */
char* dhcpc_get_client(bool ipv4,
                       const char* intf_path,
                       const char* intf_alias) {
    SAH_TRACEZ_IN(ME);
    amxc_string_t test_path;
    amxc_string_t upper_test_path;
    amxc_string_t param;
    amxb_bus_ctx_t* ctx = ipv4 ? dhcpv4_get_context() : dhcpv6_get_context();
    amxc_var_t dhcp_data;
    amxc_var_t* instance = NULL;
    amxd_status_t rv;
    char ip = ipv4 ? '4' : '6';
    char* path = NULL;

    amxc_string_init(&test_path, 0);
    amxc_string_setf(&test_path, "DHCPv%c.Client.[Interface=='%s'].", ip, intf_path);
    amxc_string_init(&upper_test_path, 0);
    amxc_string_setf(&upper_test_path, "DHCPv%c.Client.", ip);
    amxc_string_init(&param, 0);
    amxc_var_init(&dhcp_data);

    path = component_get_path_instance(ctx, amxc_string_get(&test_path, 0));

    if((NULL == path) && (NULL != intf_alias) && !ipv4) {
        SAH_TRACEZ_WARNING(ME, "DHCPv%c.Client.[Interface=='%s']. instance does not exist, selecting the first one", ip, intf_path);

        amxb_get_instances(ctx, amxc_string_get(&upper_test_path, 0), 0, &dhcp_data, 10);
        instance = amxc_var_get_first(&dhcp_data);
        when_null_trace(instance, exit, ERROR, "Getting the DHCPv%c clients timed out", ipv4);

        if(instance != NULL) {
            amxc_string_setf(&param, "DHCPv%c.Client.%s.", ip, GETP_CHAR(instance, "0.Alias"));
            rv = component_set_str_param(amxc_string_get(&param, 0), ctx, "Interface", intf_path);
            when_failed_trace(rv, exit, ERROR, "Could not change the DHCPv%c Client Interface parameter", ip);

            path = component_get_path_instance(ctx, amxc_string_get(&test_path, 0));
            when_null_trace(path, exit, ERROR, "DHCPv%c Client interface change error", ip);
        }
    }

exit:
    amxc_var_clean(&dhcp_data);
    amxc_string_clean(&upper_test_path);
    amxc_string_clean(&test_path);
    amxc_string_clean(&param);
    SAH_TRACEZ_OUT(ME);
    return path;
}


amxd_status_t dhcpc4_enable(mode_ctrl_t mode,
                            const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    char* dhcpv4_path = NULL;
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    const char* name = GET_CHAR(parameters, "Name");
    bool default_interface = GET_BOOL(parameters, "DefaultInterface");
    char* logical_path = NULL;

    SAH_TRACEZ_INFO(ME, "Enabling DHCPv4");
    when_str_empty_trace(intf_alias, exit, ERROR, "No IP interface alias found");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "No IP interface name found");

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
        lower_layer = GET_CHAR(parameters, "VLANTermination");
    }

    // Set IP-manager LowerLayers parameter for the interface in the IPv4Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set IPv4Reference LowerLayers to '%s'", lower_layer);

    // Enable the IPv4 Address instance
    rc = ipv4_addr_toggle(intf_path, NULL, DHCP_ADDRESSING_TYPE, true);
    when_failed(rc, exit);

    // Enable IPv4 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv4 on %s", intf_path);

    // Enable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

    // Set the default route origin
    if(default_interface) {
        rc = routing_default_route_set_origin(intf_path, ROUTING_ORIGIN_DHCPV4, NULL);
        when_failed_trace(rc, exit, ERROR, "Failed to configure default IPv4 route");
    }

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv4Reference to '%s'", logical_path);

    // Get the matching DHCPv4 client
    dhcpv4_path = dhcpc_get_client(true, intf_path, intf_alias);
    // Enable the DHCPv4 Client
    when_str_empty_trace(dhcpv4_path, exit, ERROR, "No DHCPv4 client found with Interface='%s'", intf_path);
    SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
    rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), true);

exit:
    free(dhcpv4_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dhcpc4_disable(mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    char* dhcpv4_path = NULL;
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    SAH_TRACEZ_INFO(ME, "Disabling DHCPv4");
    when_str_empty(intf_path, exit);
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    // Get the matching DHCPv4 client
    dhcpv4_path = dhcpc_get_client(true, intf_path, NULL);
    // Disable the DHCPv4 Client
    if(dhcpv4_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
        rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), false);
        when_failed(rc, exit);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv4 client found with Interface='%s'", intf_path);
    }

    //Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv4Reference from '%s.LowerLayers'", logical_path);

    // Disable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the whole IP interface %s", intf_path);

    // Disable IPv4 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPv4 on %s", intf_path);

    // Disable the IPv4 Address instance
    rc = ipv4_addr_toggle(intf_path, NULL, DHCP_ADDRESSING_TYPE, false);
    when_failed(rc, exit);

    // Clear the IP-manager LowerLayers parameter for the interface in the IPv4Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

exit:
    free(dhcpv4_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dhcpc6_enable(mode_ctrl_t mode,
                            const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    char* dhcpv6_path = NULL;
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    char* route_path = routing_get_interfacesetting(intf_path);
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;
    const char* router_info = "Device.Routing.RouteInformation.";
    const char* nd_intf = "wan";

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

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
        lower_layer = GET_CHAR(parameters, "VLANTermination");
    }

    // Set IP-manager LowerLayers parameter for the interface in the IPv6Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set IPv6Reference LowerLayers to '%s'", lower_layer);

    // Enable IPv6 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv6 on %s", intf_path);

    // Enable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the IP interface %s", intf_path);

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv6Reference to '%s'", logical_path);

    // Get the matching DHCPv6 client
    dhcpv6_path = dhcpc_get_client(false, intf_path, intf_alias);
    // Enable the DHCPv6 Client
    if(dhcpv6_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv6 path for %s -> %s", intf_path, dhcpv6_path);
        rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), true);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv6 client found with Interface='%s'", intf_path);
        rc = amxd_status_ok;
    }

    // Enable NeighborDiscovery for the wan
    nd_interface_setting_toggle(nd_intf, true);

exit:
    free(route_path);
    free(dhcpv6_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dhcpc6_disable(mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    char* dhcpv6_path = NULL;
    char* route_path = routing_get_interfacesetting(intf_path);
    const char* router_info = "Device.Routing.RouteInformation.";
    const char* nd_intf = "wan";
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    SAH_TRACEZ_INFO(ME, "Disabling DHCPv6");
    when_str_empty_trace(intf_path, exit, ERROR, "Interface path is empty");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    // Disable NeighborDiscovery for the wan
    nd_interface_setting_toggle(nd_intf, false);

    // Get the matching DHCPv6 client
    dhcpv6_path = dhcpc_get_client(false, intf_path, NULL);
    // Disable the DHCPv6 Client
    if(dhcpv6_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv6 path for %s -> %s", intf_path, dhcpv6_path);
        rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);
        when_failed(rc, exit);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv6 client found with Interface='%s'", intf_path);
    }

    // Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv6Reference from '%s'", logical_path);

    // Disable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the IP interface %s", intf_path);

    // Disable IPv6 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPv6 on %s", intf_path);

    // Make the lower layer of the IP-Manager's interface empty
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

    // Disable the RouteInformation instance
    rc = component_set_enable(router_info, routing_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disble the Routing manager's RoutingInformation");

    // Empty the interface reference of the RouteInformation
    rc = component_set_str_param(route_path, routing_get_context(), "Interface", "");
    when_failed_trace(rc, exit, ERROR, "Failed to empty the Routing manager's interface");

exit:
    free(route_path);
    free(dhcpv6_path);
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}
