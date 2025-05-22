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
#include "component.h"
#include "wan_manager_utils.h"
#include "bridge_mode.h"

#define ME "bridge-mode"

static amxd_status_t bridge_mode_none_enable(const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* prefixed_intf_path = get_ip_path(parameters, IPv4, true);
    const char* intf_path = get_ip_path(parameters, IPv4, false);
    const char* lower_layer = GET_CHAR(parameters, "LowerLayer");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(prefixed_intf_path, exit, ERROR, "No prefixed IP interface path found");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "No IP interface name found");

    // Set IP-manager LowerLayers parameter for the interface in the IPv4Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed_trace(rc, exit, ERROR, "Failed to set '%s' LowerLayers to '%s'", intf_path, lower_layer);

    // Enable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable IP interface '%s'", intf_path);

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", prefixed_intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add '%s' to '%s'", intf_path, logical_path);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t bridge_mode_none_disable(const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* prefixed_intf_path = get_ip_path(parameters, IPv4, true);
    const char* intf_path = get_ip_path(parameters, IPv4, false);
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(prefixed_intf_path, exit, ERROR, "No prefixed IP interface path found");
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    //Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", prefixed_intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove '%s' from '%s.LowerLayers'", prefixed_intf_path, logical_path);

    // Disable the IP interface
    rc = component_set_enable(intf_path, ip_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the whole IP interface '%s'", intf_path);

    // Clear the IP-manager LowerLayers parameter for the interface in the IPv4Reference parameter
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed_trace(rc, exit, ERROR, "Failed to clear LowerLayers for '%s'", intf_path);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t bridge_mode_ctrl_action(const amxc_var_t* const parameters,
                                      bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = bridge_mode_none_enable(parameters);
    } else {
        rc = bridge_mode_none_disable(parameters);
    }

    SAH_TRACEZ_OUT(ME);
    return rc;
}

int manage_bridge(const char* bridge_reference, const char* lower_layer, bool enable, mode_ctrl_t mode, uint32_t vlan_id, uint32_t vlan_prio) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxb_bus_ctx_t* bus_ctx = amxb_be_who_has("Bridging.");
    amxc_var_t params;
    amxc_var_t ret;
    const char* method = enable ? "AddPort" : "DisablePort";

    amxc_var_init(&params);
    amxc_var_init(&ret);

    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &params, "LowerLayers", lower_layer);
    amxc_var_add_key(bool, &params, "Enable", enable);
    if((mode & TYPE_VLAN) != 0) {
        amxc_var_add_key(uint32_t, &params, "VlanId", vlan_id);
        amxc_var_add_key(uint32_t, &params, "VlanPriority", vlan_prio);
    }

    rv = amxb_call(bus_ctx, bridge_reference, method, &params, &ret, 5);
    when_failed_trace(rv, exit, ERROR, "Failed to call '%s' on '%s', return '%d'", method, bridge_reference, rv);

exit:
    amxc_var_clean(&ret);
    amxc_var_clean(&params);
    SAH_TRACEZ_OUT(ME);
    return rv;
}
