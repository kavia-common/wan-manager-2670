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
#include "ppp/ppp.h"
#include "ethernet/ethernet.h"
#include "component.h"
#include "wan_manager_utils.h"

#define ME "ppp-ctrl"

#define MASK_PPP_IPvX ((MASK_IPv4 | MASK_IPv6) & MASK_PPP)

static amxd_status_t ppp_enable_lower(mode_ctrl_t mode,
                                      amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    const ipversion_t ip_version = get_ipversion(mode & MASK_PPP_IPvX);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    const char* prefixed_intf_path = get_ip_path(parameters, ip_version, true);
    const char* ppp_path = get_ppp_path(parameters, ip_version, false);
    const char* prefixed_ppp_path = get_ppp_path(parameters, ip_version, true);
    const char* username = GET_CHAR(parameters, "UserName");
    const char* password = GET_CHAR(parameters, "Password");

    when_false(ipversion_valid(ip_version), exit);
    SAH_TRACEZ_INFO(ME, "Enabling PPP%d", ip_version);

    when_str_empty_trace(prefixed_intf_path, exit, ERROR, "Failed to get/create IP interface path");
    when_str_empty_trace(ppp_path, exit, ERROR, "Failed to get/create PPP instance path");
    when_str_empty_trace(prefixed_ppp_path, exit, ERROR, "Failed to get/create prefixed PPP instance path");

    // Set LowerLayer in PPP-manager
    rc = component_set_str_param(ppp_path, ppp_get_context(), "LowerLayers", lower_layer);
    when_failed(rc, exit);

    // Only override default PPP credentials if they are set in our Datamodel
    if(!str_empty(username)) {
        rc = component_set_str_param(ppp_path, ppp_get_context(), "Username", username);
        when_failed(rc, exit);
    }
    if(!str_empty(password)) {
        rc = component_set_str_param(ppp_path, ppp_get_context(), "Password", password);
        when_failed(rc, exit);
    }

    if(ip_version == IPv4) {
        amxc_var_add_key(cstring_t, parameters, "LowerLayersV4Override", prefixed_ppp_path);

        // Enable PPPv4
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPCPEnable", true);
        when_failed(rc, exit);

        // Disable PPPv6
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPv6CPEnable", false);
        when_failed(rc, exit);

    } else {
        amxc_var_add_key(cstring_t, parameters, "LowerLayersV6Override", prefixed_ppp_path);

        // Disable PPPv4
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPCPEnable", false);
        when_failed(rc, exit);

        // Enable PPPv6
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPv6CPEnable", true);
        when_failed(rc, exit);
    }

    // Enable the PPP interface
    rc = component_set_enable(ppp_path, ppp_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable PPP instance '%s'", ppp_path);

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t ppp_enable_upper(mode_ctrl_t mode,
                                      const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    const ipversion_t ip_version = get_ipversion(mode & MASK_PPP_IPvX);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* prefixed_intf_path = NULL;
    char* dhcpv6_path = NULL;
    const char* neigh_disc_path = NULL;

    when_false(ipversion_valid(ip_version), exit);
    when_false_status(ip_version == IPv6, exit, rc = amxd_status_ok);

    prefixed_intf_path = get_ip_path(parameters, IPv6, true);
    when_str_empty_trace(prefixed_intf_path, exit, ERROR, "Failed to get IP interface path");

    // Enable NeighborDiscovery for the wan
    neigh_disc_path = get_nd_path(parameters);
    rc = component_set_enable(neigh_disc_path, neighbor_discovery_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable NeighborDiscovery");

    // Enable the DHCPv6 Client
    dhcpv6_path = get_dhcp_path(parameters, IPv6);
    when_str_empty_trace(dhcpv6_path, exit, ERROR, "Failed to get DHCPv6 client instance path");
    rc = component_set_str_param(dhcpv6_path, dhcpv6_get_context(), "Interface", prefixed_intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to set " DHCPV6_REFERENCE_PATH " Interface to '%s'", prefixed_intf_path);
    rc = component_set_bool(dhcpv6_path, dhcpv6_get_context(), "RequestPrefixes", true);
    when_failed_trace(rc, exit, ERROR, "Failed to configure DHCPv6 IA_PD");
    rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the DHCPv6 Client");

    rc = amxd_status_ok;

exit:
    free(dhcpv6_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t ppp_disable_lower(mode_ctrl_t mode,
                                       const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const ipversion_t ip_version = get_ipversion(mode & MASK_PPP_IPvX);
    const char* prefixed_intf_path = NULL;
    const char* ppp_path = NULL;

    when_false(ipversion_valid(ip_version), exit);
    SAH_TRACEZ_INFO(ME, "Disabling PPP%d", ip_version);

    prefixed_intf_path = get_ip_path(parameters, ip_version, true);
    ppp_path = get_ppp_path(parameters, ip_version, false);
    when_str_empty_trace(prefixed_intf_path, exit, ERROR, "Failed to get IP interface path");
    when_str_empty_trace(ppp_path, exit, ERROR, "Failed to get PPP instance path");

    // Disable the PPP interface
    rc = component_set_enable(ppp_path, ppp_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable PPP instance '%s'", ppp_path);

    if(ip_version == IPv4) {
        // Disable PPPv4
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPCPEnable", false);
        when_failed(rc, exit);
    } else {
        // Disable PPPv6
        rc = component_set_bool(ppp_path, ppp_get_context(), "IPv6CPEnable", false);
        when_failed(rc, exit);
    }

    // Clear LowerLayer in PPP-manager
    rc = component_set_str_param(ppp_path, ppp_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear '%s.LowerLayers'", ppp_path);

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t ppp_disable_upper(mode_ctrl_t mode,
                                       const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const ipversion_t ip_version = get_ipversion(mode & MASK_PPP_IPvX);
    char* dhcpv6_path = NULL;
    const char* intf_path = NULL;
    const char* neigh_disc_path = NULL;

    when_false(ipversion_valid(ip_version), exit);
    when_false_status(ip_version == IPv6, exit, rc = amxd_status_ok);

    intf_path = get_ip_path(parameters, IPv6, true);
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");

    // Disable NeighborDiscovery for the wan
    neigh_disc_path = get_nd_path(parameters);
    rc = component_set_enable(neigh_disc_path, neighbor_discovery_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable NeighborDiscovery");

    // Disable the DHCPv6 Client
    dhcpv6_path = get_dhcp_path(parameters, IPv6);
    when_str_empty_trace(dhcpv6_path, exit, ERROR, "Failed to get DHCPv6 client instance path");
    SAH_TRACEZ_INFO(ME, "DHCPv6 path for %s -> %s", intf_path, dhcpv6_path);
    rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the DHCPv6 Client");
    rc = component_set_str_param(dhcpv6_path, dhcpv6_get_context(), "Interface", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear " DHCPV6_REFERENCE_PATH " Interface");

exit:
    free(dhcpv6_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t ppp_lower_layer(mode_ctrl_t mode,
                              amxc_var_t* const parameters,
                              bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = ppp_enable_lower(mode, parameters);
    } else {
        rc = ppp_disable_lower(mode, parameters);
    }

    return rc;
}

amxd_status_t ppp_upper_layer(mode_ctrl_t mode,
                              amxc_var_t* const parameters,
                              bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = ppp_enable_upper(mode, parameters);
    } else {
        rc = ppp_disable_upper(mode, parameters);
    }

    return rc;
}
