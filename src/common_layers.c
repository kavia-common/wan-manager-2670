/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice,
** this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** Subject to the terms and conditions of this license, each copyright holder
** and contributor hereby grants to those receiving rights under this license
** a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable
** (except for failure to satisfy the conditions of this license) patent license
** to make, have made, use, offer to sell, sell, import, and otherwise transfer
** this software, where such license applies only to those patent claims, already
** acquired or hereafter acquired, licensable by such copyright holder or contributor
** that are necessarily infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright holders and
** non-copyrightable additions of contributors, in source or binary form) alone;
** or
**
** (b) combination of their Contribution(s) with the work of authorship to which
** such Contribution(s) was added by such copyright holder or contributor, if,
** at the time the Contribution is added, such addition causes such combination
** to be necessarily infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any copyright
** holder or contributor is granted under this license, whether expressly, by
** implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
** USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "ctrl/mode_ctrl.h"
#include "component.h"
#include "wan_manager_utils.h"
#include "common_layers.h"

#define ME "common-layer"

/**
 * @brief Convert the mode_ctrl to a IPv4 Addressing type
 * @param mode The mode_ctrl value for the mode you want to convert
 * @return a string containing the IPv4 addressing type, otherwise NULL is returned
 */
static const char* get_addressing_v4_type(mode_ctrl_t mode) {
    const char* addr_type = NULL;
    switch(mode & MASK_IPv4) {
    case IPv4_DHCP:
        addr_type = DHCP_ADDRESSING_TYPE;
        break;
    case IPv4_PPP:
        addr_type = PPP_ADDRESSING_TYPE;
        break;
    case IPv4_STATIC:
        addr_type = STATIC_ADDRESSING_TYPE;
        break;
    default:
        break;
    }

    return addr_type;
}

/**
 * @brief Convert the mode_ctrl to a IPv4 routing origin
 * @param mode The mode_ctrl value for the mode you want to convert
 * @return a string containing the IPv4 routing origin, otherwise NULL is returned
 */
static const char* get_routing_v4_origin(mode_ctrl_t mode) {
    const char* routing_origin = NULL;

    switch(mode & MASK_IPv4) {
    case IPv4_DHCP:
        routing_origin = ROUTING_ORIGIN_DHCPV4;
        break;
    case IPv4_PPP:
        routing_origin = ROUTING_ORIGIN_IPCP;
        break;
    case IPv4_STATIC:
        routing_origin = ROUTING_ORIGIN_STATIC;
        break;
    case IPv4_DSLITE:
        routing_origin = ROUTING_ORIGIN_STATIC;
        break;
    default:
        break;
    }

    return routing_origin;
}

/**
 * @brief Configures IPv4 on the IP interface referenced in IPv4Reference
 * @param mode The mode_ctrl value for the mode you want to apply
 * @param parameters variant containing the mode parameters
 * @return 0 when the config was applied correctly, an error value otherwise
 */
static amxd_status_t ipv4_set_config(mode_ctrl_t mode,
                                     const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayersV4Override"); // Some modes might have a LowerLayers override, example ppp
    const char* addr_type = get_addressing_v4_type(mode);
    amxc_var_t* ipv4_params = GET_ARG(parameters, "ipv4");

    if(str_empty(lower_layer)) {
        lower_layer = GET_CHAR(parameters, "LowerLayer"); // If no Override was provided by an earlier layer, use lowerlayer
    }

    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set '%s' LowerLayers to '%s'", intf_path, lower_layer);

    rc = ipv4_addr_toggle(intf_path, ipv4_params, addr_type, true);
    when_failed_trace(rc, exit, ERROR, "Failed to Enable the IPv4 address %s", intf_path);

    rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv4 on %s", intf_path);

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Clears the IPv4 on the IP interface referenced in IPv4Reference
 * @param mode The mode_ctrl value for the mode you want to break down
 * @param parameters variant containing the mode parameters
 * @return 0 when the config was cleared correctly, an error value otherwise
 */
static amxd_status_t ipv4_clear_config(mode_ctrl_t mode,
                                       const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* addr_type = get_addressing_v4_type(mode);

    rc = component_set_bool(intf_path, ip_get_context(), "IPv4Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPv4 on %s", intf_path);

    rc = ipv4_addr_toggle(intf_path, NULL, addr_type, false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the IPv4 address %s", intf_path);

    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear IPv4Reference LowerLayers");

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Configures IPv6 on the IP interface referenced in IPv6Reference
 * @param parameters variant containing the mode parameters
 * @return 0 when the config was applied correctly, an error value otherwise
 */
static amxd_status_t ipv6_set_config(mode_ctrl_t mode,
                                     const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayersV6Override");
    const char* old_intf_path = GETP_CHAR(parameters, "old_interface_parameters.IPv6Reference");
    const char* deferred_ipv6_instances = GET_CHAR(parameters, "DeferredIPv6Instances");
    const char* ipv6_address_delegate = GET_CHAR(parameters, "IPv6AddressDelegate");

    if(str_empty(lower_layer)) {
        lower_layer = GET_CHAR(parameters, "LowerLayer");
    }

    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set '%s' LowerLayers to '%s'", intf_path, lower_layer);

    ip_parent_prefix_toggle(deferred_ipv6_instances, old_intf_path, intf_path);

    if(!str_empty(ipv6_address_delegate)) { // Unnumbered mode
        rc = component_set_str_param(intf_path, ip_get_context(), "IPv6AddressDelegate", ipv6_address_delegate);
        when_failed_trace(rc, exit, ERROR, "Failed to set IPv6AddressDelegate on '%s'", intf_path);

        rc = nd_interface_setting_toggle(NEIGH_DISCOVERY_INTF, "AutoConfEnable", false);
        when_failed_trace(rc, exit, ERROR, "Failed to disable AutoConf");
    }

    if((mode & MASK_IPv6) == IPv6_STATIC) {
        amxc_var_t* ipv6 = GET_ARG(parameters, "ipv6");
        rc = ipv6_addr_toggle(intf_path, ipv6, STATIC_ADDRESSING_TYPE, true);
        when_failed_trace(rc, exit, ERROR, "Failed to set the static ipv6 address in '%s'", intf_path);
    }

    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv6 on '%s'", intf_path);

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Clears the IPv6 on the IP interface referenced in IPv6Reference
 * @param parameters variant containing the mode parameters
 * @return 0 when the config was cleared correctly, an error value otherwise
 */
static amxd_status_t ipv6_clear_config(mode_ctrl_t mode,
                                       const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv6Reference");

    rc = component_set_enable(intf_path, ip_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the IP interface %s", intf_path);

    if((mode & MASK_IPv6) == IPv6_STATIC) {
        amxc_var_t* ipv6 = GET_ARG(parameters, "ipv6");
        rc = ipv6_addr_toggle(intf_path, ipv6, STATIC_ADDRESSING_TYPE, false);
        when_failed_trace(rc, exit, ERROR, "Failed to unset the static ipv6 address in '%s'", intf_path);
    }

    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPv6 on %s", intf_path);

    rc = component_set_str_param(intf_path, ip_get_context(), "IPv6AddressDelegate", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear %s.IPv6AddressDelegate", intf_path);

    rc = nd_interface_setting_toggle(NEIGH_DISCOVERY_INTF, "AutoConfEnable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable AutoConf");

    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear IPv6Reference LowerLayers");

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Sets or clears the IPv4 configuration, the IP instance will NOT be toggled by this function
 * @param mode The mode_ctrl value for the mode you want to configure
 * @param parameters variant containing the mode parameters
 * @param enable boolean to control if the configuration should be set or cleared
 * @return 0 when the config was set or cleared correctly, an error value otherwise
 */
amxd_status_t ipv4_layer(mode_ctrl_t mode,
                         amxc_var_t* const parameters,
                         bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = ipv4_set_config(mode, parameters);
    } else {
        rc = ipv4_clear_config(mode, parameters);
    }
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Sets or clears the IPv6 configuration, the IP instance will NOT be toggled by this function
 * @param mode The mode_ctrl value for the mode you want to configure
 * @param parameters variant containing the mode parameters
 * @param enable boolean to control if the configuration should be set or cleared
 * @return 0 when the config was set or cleared correctly, an error value otherwise
 */
amxd_status_t ipv6_layer(UNUSED mode_ctrl_t mode,
                         amxc_var_t* const parameters,
                         bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = ipv6_set_config(mode, parameters);
    } else {
        rc = ipv6_clear_config(mode, parameters);
    }
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Toggles the enable parameter of the IPv4 or IPv6 reference instance
 * @param mode The mode_ctrl value for the mode you want to configure
 * @param parameters variant containing the mode parameters
 * @param enable boolean to control if the instance should be enabled or disabled
 * @return 0 when the instance was toggled correctly, an error value otherwise
 */
amxd_status_t ip_enable(mode_ctrl_t mode,
                        amxc_var_t* const parameters,
                        bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = ((mode & MASK_IPv4) != 0) ? GET_CHAR(parameters, "IPv4Reference") : GET_CHAR(parameters, "IPv6Reference");

    rc = component_set_enable(intf_path, ip_get_context(), enable);
    when_failed_trace(rc, exit, ERROR, "Failed to '%s' '%s'", enable ? "enable" : "disable", intf_path);

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Will add the IPv4/6Reference to the correct logical interface, for IPv4 it will also configure the default route
 * @param mode The mode_ctrl value for the mode you want to configure
 * @param parameters variant containing the mode parameters
 * @param enable boolean to control if the configuration should be set or cleared
 * @return 0 when the config was set or cleared correctly, an error value otherwise
 */
amxd_status_t logical_layer(mode_ctrl_t mode,
                            amxc_var_t* const parameters,
                            bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    int ipmode = mode & (MASK_IPv4 | MASK_IPv6);
    int ip_version = ((ipmode & MASK_IPv4) != 0) ? 4 : 6;
    const char* intf_path = ip_version == 4 ? GET_CHAR(parameters, "IPv4Reference") : GET_CHAR(parameters, "IPv6Reference");
    const char* name = GET_CHAR(parameters, "Name");
    const char* default_route_reference = GET_CHAR(parameters, "DefaultRouteReference");
    char* logical_path = NULL;

    // Set the default route origin
    if(enable && (ip_version == 4) && !str_empty(default_route_reference)) {
        const char* routing_origin = get_routing_v4_origin(mode);
        rc = routing_default_route_set_origin(default_route_reference, intf_path, routing_origin, NULL);
        when_failed_trace(rc, exit, ERROR, "Failed to configure default IPv4 route");
    }

    logical_path = create_logical_path(name);
    if(enable) {
        rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    } else {
        rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    }
    when_failed_trace(rc, exit, ERROR, "Failed to %s IPv%dReference to '%s'", enable ? "add" : "remove", ip_version, logical_path);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}
