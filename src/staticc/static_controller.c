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
#include <amxd/amxd_object.h>

#include "ctrl/mode_ctrl.h"
#include "staticc/static_controller.h"
#include "ethernet/ethernet.h"
#include "component.h"
#include "wan_manager_utils.h"
#include "dm_wan-manager.h"
#include "dhcpc/dhcpc.h"

#define ME "static-ctrl"

amxd_status_t static4_enable(UNUSED mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* old_intf_path = GETP_CHAR(parameters, "old_interface_parameters.IPv6Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    const char* default_route_reference = GET_CHAR(parameters, "DefaultRouteReference");
    amxc_var_t* ipv4 = GET_ARG(parameters, "ipv4");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "No IP interface name found");

    // Set IP-manager LowerLayers parameter for the interface in the IPv4Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set IPv4Reference LowerLayers to '%s'", lower_layer);

    // Switch the parent prefix path to the correct IPv6Reference
    ip_parent_prefix_toggle(GET_CHAR(parameters, "DeferredIPv6Instances"), old_intf_path, intf_path);

    // Enable the IPv4 Address instance
    rc = ipv4_addr_toggle(intf_path, ipv4, STATIC_ADDRESSING_TYPE, true);
    when_failed(rc, exit);

    // Enable IPv4 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv4 on %s", intf_path);

    // Enable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

    // Setting up the default route instance for static ipv4
    if(!str_empty(default_route_reference)) {
        rc = routing_default_route_set_origin(default_route_reference, intf_path, ROUTING_ORIGIN_STATIC, GET_CHAR(ipv4, "DefaultRouter"));
        when_failed_trace(rc, exit, ERROR, "Failed to configure default IPv4 route");
    }

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv4Reference to '%s'", logical_path);

    if((strcmp(name, "wan") == 0)) {
        amxd_object_t* interface = amxd_dm_findf(wan_get_dm(), "%s", GET_CHAR(parameters, "intf_obj_path"));
        amxd_object_t* wan_mode_obj = amxd_object_findf(interface, "^.^.");

        if(amxd_object_set_value(cstring_t, wan_mode_obj, "DNSMode", "Static")) {
            SAH_TRACEZ_ERROR(ME, "Failed to set DNSMode to 'Static'");
        }
    }

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t static4_disable(UNUSED mode_ctrl_t mode,
                              const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

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
    rc = ipv4_addr_toggle(intf_path, NULL, STATIC_ADDRESSING_TYPE, false);
    when_failed(rc, exit);

    // Empty the LowerLayers of the ip interface
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t static6_enable(UNUSED mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    amxc_var_t* ipv6 = GET_ARG(parameters, "ipv6");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    // Set IP-manager LowerLayers parameter for the interface in the IPv6Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set IPv6Reference LowerLayers to '%s'", lower_layer);

    // Enable the IPv6 Address instance
    rc = ipv6_addr_toggle(intf_path, ipv6, STATIC_ADDRESSING_TYPE, true);
    when_failed_trace(rc, exit, ERROR, "Failed to set the static ipv6 address in interface %s", intf_path);

    // Enable IPv6 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv6 on %s", intf_path);

    // Enable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv6Reference to '%s'", logical_path);

    // Setting up the default route instance
    rc = routing_default_ipv6_route_mod_inst(ROUTING_ORIGIN_STATIC, GET_CHAR(ipv6, "DefaultRouter"), intf_path, true);
    when_failed_trace(rc, exit, ERROR, "Failed to set the static default IPv6 route");

    if((strcmp(name, "wan") == 0)) {
        amxd_object_t* interface = amxd_dm_findf(wan_get_dm(), "%s", GET_CHAR(parameters, "intf_obj_path"));
        amxd_object_t* wan_mode_obj = amxd_object_findf(interface, "^.^.");

        if(amxd_object_set_value(cstring_t, wan_mode_obj, "IPv6DNSMode", "Static")) {
            SAH_TRACEZ_ERROR(ME, "Failed to set IPv6DNSMode to 'Static'");
        }
    }

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t static6_disable(UNUSED mode_ctrl_t mode,
                              const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    amxc_var_t* ipv6 = GET_ARG(parameters, "ipv6");
    const char* default_router = GET_CHAR(ipv6, "DefaultRouter");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    // Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv6Reference from '%s'", logical_path);

    // Disable the default route instance
    rc = routing_default_ipv6_route_mod_inst(ROUTING_ORIGIN_STATIC, default_router, intf_path, false);
    when_failed_trace(rc, exit, ERROR, "Failed to remove the default route");

    // Disable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the whole IP interface %s", intf_path);

    // Disable the IPv6 Address instance
    rc = ipv6_addr_toggle(intf_path, ipv6, STATIC_ADDRESSING_TYPE, false);
    when_failed_trace(rc, exit, ERROR, "Failed to unset the static ipv6 address in interface %s", intf_path);

    // Disable IPv6 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPv6 on %s", intf_path);

    // Emptying the LowerLayer parameter of the IP-manager's interface
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to unset the lower layer in interface %s", intf_path);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}
