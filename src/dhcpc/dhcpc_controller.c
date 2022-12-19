/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2021 SoftAtHome
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
    amxc_string_t query;
    amxc_string_t upper_query;
    amxc_string_t param;
    amxb_bus_ctx_t* ctx = ipv4 ? dhcpv4_get_context() : dhcpv6_get_context();
    amxc_var_t dhcp_data;
    amxc_var_t* instance = NULL;
    amxd_status_t rv;
    char ip = ipv4 ? '4' : '6';
    char* path = NULL;

    amxc_string_init(&query, 0);
    amxc_string_setf(&query, "DHCPv%c.Client.[Interface=='%s'].", ip, intf_path);
    amxc_string_init(&upper_query, 0);
    amxc_string_setf(&upper_query, "DHCPv%c.Client.", ip);
    amxc_string_init(&param, 0);
    amxc_var_init(&dhcp_data);

    path = component_get_path_instance(ctx, amxc_string_get(&query, 0));

    if((NULL == path) && (NULL != intf_alias) && !ipv4) {
        SAH_TRACEZ_INFO(ME, "DHCPv%c.Client.[Interface=='%s']. instance does not exist." \
                        " Selecting the first one", ip, intf_path);

        amxb_get_instances(ctx, amxc_string_get(&upper_query, 0), 0, &dhcp_data, 10);
        instance = amxc_var_get_first(&dhcp_data);
        when_null_trace(instance, exit, ERROR, "Getting the DHCPv%c clients timed out", ipv4);

        if(instance != NULL) {
            amxc_string_setf(&param, "DHCPv%c.Client.%s.", ip, GETP_CHAR(amxc_var_get_first(instance), "Alias"));
            rv = component_set_str_param(amxc_string_get(&param, 0), ctx, "Interface", intf_path);
            when_failed_trace(rv, skip, ERROR, "Could not change the Interface parameter");

            path = component_get_path_instance(ctx, amxc_string_get(&query, 0));
            when_null_trace(path, skip, ERROR, "DHCPv%c Client interface change error", ip);

            goto exit;
        }
skip:
        when_null_trace(path, exit, ERROR, "DHCPv%c Client Interface error", ip);
    }
exit:
    amxc_var_clean(&dhcp_data);
    amxc_string_clean(&upper_query);
    amxc_string_clean(&query);
    amxc_string_clean(&param);
    return path;
}

amxd_status_t dhcpc4_enable(mode_ctrl_t mode,
                            const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    char* dhcpv4_path = NULL;
    const char* intf_alias = GETP_CHAR(parameters, "Alias");
    const char* intf_path = GETP_CHAR(parameters, "IPv4Reference");
    const char* lower_layer = GETP_CHAR(parameters, "LowerLayer");

    when_str_empty_trace(intf_alias, exit, ERROR, "No IP interface alias found");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
        lower_layer = GETP_CHAR(parameters, "VLANTermination");
    }

    // Enable the correct IPv4 Address instance
    rc = ip_addr_toggle(intf_path, DHCP_ADDRESSING_TYPE, true);
    when_failed(rc, exit);
    // Set IP-manager LowerLayers parameter for the interface in the IPv4Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set IPv4Reference LowerLayers to '%s'", lower_layer);

    routing_default_route_set_origin(intf_path, ROUTING_ORIGIN_DHCPV4);

    //Enable the whole interface
    rc = component_set_bool(intf_path, ip_get_context(), "Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

    // Get the matching DHCPv4 client
    dhcpv4_path = dhcpc_get_client(true, intf_path, intf_alias);

    // Enable the DHCPv4 Client
    if(dhcpv4_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
        rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), true);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv4 client found with Interface='%s'", intf_path);
        rc = amxd_status_ok;
    }

exit:
    free(dhcpv4_path);
    return rc;
}

amxd_status_t dhcpc4_disable(mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GETP_CHAR(parameters, "IPv4Reference");
    char* dhcpv4_path = NULL;

    when_str_empty(intf_path, exit);

    dhcpv4_path = dhcpc_get_client(true, intf_path, NULL);
    if(dhcpv4_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
        rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), false);
        when_failed(rc, exit);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv4 client found with Interface='%s'", intf_path);
    }

    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

    // Disable the correct IPv4 Address instance
    rc = ip_addr_toggle(intf_path, DHCP_ADDRESSING_TYPE, false);
    when_failed(rc, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

exit:
    free(dhcpv4_path);
    return rc;
}

amxd_status_t dhcpc6_enable(mode_ctrl_t mode,
                            const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    char* dhcpv6_path = NULL;
    const char* intf_alias = GETP_CHAR(parameters, "Alias");
    const char* intf_path = GETP_CHAR(parameters, "IPv6Reference");
    const char* lower_layer = GETP_CHAR(parameters, "LowerLayer");

    when_str_empty(intf_alias, exit);
    when_str_empty(intf_path, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
        lower_layer = GETP_CHAR(parameters, "VLANTermination");
    }

    // Set IP-manager LowerLayers parameter for the interface in the IPv6Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set IPv6Reference LowerLayers to '%s'", lower_layer);

    // Enable IPv6 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IPv6 on %s", intf_path);

    //Enable the whole interface
    rc = component_set_bool(intf_path, ip_get_context(), "Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

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

exit:
    free(dhcpv6_path);
    return rc;
}

amxd_status_t dhcpc6_disable(mode_ctrl_t mode,
                             const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GETP_CHAR(parameters, "IPv6Reference");
    char* dhcpv6_path = NULL;

    when_str_empty(intf_path, exit);

    dhcpv6_path = dhcpc_get_client(false, intf_path, NULL);
    if(dhcpv6_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv6 path for %s -> %s", intf_path, dhcpv6_path);
        rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);
        when_failed(rc, exit);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv6 client found with Interface='%s'", intf_path);
    }

    // Disable IPv6 on the IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "IPv6Enable", false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPv6 on %s", intf_path);

    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

exit:
    free(dhcpv6_path);
    return rc;
}
