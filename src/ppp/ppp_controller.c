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
    // For now a fixed path to the first instance is used, in the future it should be possible to get a specific instance
    return "Device.PPP.Interface.1.";
}

amxd_status_t ppp_enable(mode_ctrl_t mode,
                         const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* ppp_path = NULL;
    const char* intf_alias = GET_CHAR(parameters, "Alias");
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* username = GET_CHAR(parameters, "UserName");
    const char* password = GET_CHAR(parameters, "Password");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    SAH_TRACEZ_INFO(ME, "Enabling PPP4");
    when_str_empty_trace(intf_alias, exit, ERROR, "No IP interface alias found");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter for interface %s is emtpy", intf_path);

    // Get the matching PPP client
    ppp_path = ppp_get_client(true, intf_path, intf_alias);

    // VLANS
    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
        lower_layer = GETP_CHAR(parameters, "VLANTermination");
    }

    // Setup and Enable PPP
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

    rc = component_set_enable(ppp_path, ppp_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable PPP instance '%s'", ppp_path);

    // Set LowerLayers path in IP-manager to the PPP instance
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", ppp_path);
    when_failed_trace(rc, exit, ERROR, "Failed to set '%s.LowerLayers' to '%s'", intf_path, ppp_path);

    // Enable the correct IPv4 Address instance
    rc = ip_addr_toggle(intf_path, PPP_ADDRESSING_TYPE, true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the correct IPv4 Address instance");

    // Set the default route origin
    routing_default_route_set_origin(intf_path, ROUTING_ORIGIN_IPCP);

    // Enable the right IP interface
    rc = component_set_bool(intf_path, ip_get_context(), "Enable", true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable the whole IP interface %s", intf_path);

    //Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv4Reference to '%s'", logical_path);


    rc = amxd_status_ok;

exit:
    free(logical_path);
    return rc;
}

amxd_status_t ppp_disable(mode_ctrl_t mode,
                          const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = GET_CHAR(parameters, "IPv4Reference");
    const char* ppp_path = ppp_get_client(true, intf_path, NULL);
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    SAH_TRACEZ_INFO(ME, "Disabling PPP4");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(ppp_path, exit, ERROR, "No PPP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    //Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv4Reference from '%s.LowerLayers'", logical_path);

    // Disable PPP
    rc = component_set_enable(ppp_path, ppp_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable PPP instance '%s'", ppp_path);

    // Clear the LowerLayers parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear '%s.LowerLayers'", intf_path);
    rc = component_set_str_param(ppp_path, ppp_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear '%s.LowerLayers'", ppp_path);

    // Disable the IPv4 address instance
    rc = ip_addr_toggle(intf_path, PPP_ADDRESSING_TYPE, false);
    when_failed(rc, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

exit:
    free(logical_path);
    return rc;
}

amxd_status_t ppp6_enable(UNUSED mode_ctrl_t mode,
                          UNUSED const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;

    SAH_TRACEZ_INFO(ME, "Enabling PPP6");
    rc = amxd_status_ok;
    return rc;
}

amxd_status_t ppp6_disable(UNUSED mode_ctrl_t mode,
                           UNUSED const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;

    SAH_TRACEZ_INFO(ME, "Disabling PPP6");
    rc = amxd_status_ok;
    return rc;
}
