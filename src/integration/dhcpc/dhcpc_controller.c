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

#include "dm_wan_mode.h"
#include "dm_wan-manager.h"
#include "ctrl/mode_ctrl.h"
#include "integration/dhcpc/dhcpc.h"
#include "integration/ethernet/ethernet.h"
#include "component.h"

#define ME "dhcpc-ctrl"

static mode_ctrl_actions_t dhcpc_actions;
static amxb_bus_ctx_t* dhcpv4_ctx = NULL;
static amxb_bus_ctx_t* ip_ctx = NULL;
static amxb_bus_ctx_t* ip_get_context(void);

static amxd_status_t dhcpc_enable(wan_mode_type_t mode, const amxc_var_t* const parameters);
static amxd_status_t dhcpc_disable(wan_mode_type_t mode, const amxc_var_t* const parameters);
static amxb_bus_ctx_t* dhcpv4_get_context(void);
static char* dhcpv4_add_client_instance(const char* name, const char* lower_layer);

AMXB_CONSTRUCTOR static void dhcp_controller_init(void) {
    dhcpc_actions.enable = dhcpc_enable;
    dhcpc_actions.disable = dhcpc_disable;
    (void) register_mode_controller(Untagged_DHCP, &dhcpc_actions);
    (void) register_mode_controller(Tagged_DHCP, &dhcpc_actions);
}

static amxd_status_t dhcpc_enable(wan_mode_type_t mode,
                                  const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    char* dhcpv4_path = NULL;
    const char* intf_alias = NULL;
    const char* intf_path = NULL;
    const char* lower_layer = NULL;

    when_null(parameters, exit);
    lower_layer = GETP_CHAR(parameters, "LowerLayer");

    if(Tagged_DHCP == mode) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
    }
    intf_alias = GETP_CHAR(parameters, "Alias");
    intf_path = GETP_CHAR(parameters, "IPReference");

    when_str_empty(intf_alias, exit);
    when_str_empty(intf_path, exit);

    dhcpv4_path = component_get_path_instance(dhcpv4_get_context(),
                                              "DHCPv4.Client.[Interface=='%s'].",
                                              intf_path);
    if(NULL == dhcpv4_path) {
        SAH_TRACEZ_INFO(ME, "DHCPv4.Client.[Interface=='%s']. instance does not exist." \
                        " Create it", intf_path);
        dhcpv4_path = dhcpv4_add_client_instance(intf_alias, intf_path);
        when_null_trace(dhcpv4_path, exit, ERROR, "Add DHCPv4 Client instance error");
    }
    if(Tagged_DHCP == mode) {
        lower_layer = GETP_CHAR(parameters, "VLANTermination");
    }
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed(rc, exit);
    rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), true);
exit:
    free(dhcpv4_path);
    return rc;
}

static amxd_status_t dhcpc_disable(wan_mode_type_t mode,
                                   const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = NULL;
    char* dhcpv4_path = NULL;

    when_null(parameters, exit);
    intf_path = GETP_CHAR(parameters, "IPReference");
    when_str_empty(intf_path, exit);
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

    if(Tagged_DHCP == mode) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }
    dhcpv4_path = component_get_path_instance(dhcpv4_get_context(),
                                              "DHCPv4.Client.[Interface=='%s'].",
                                              intf_path);
    if(dhcpv4_path != NULL) {
        SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
        rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), false);
    } else {
        SAH_TRACEZ_INFO(ME, "No DHCPv4 client found with Interface='%s'", intf_path);
        rc = amxd_status_ok;
    }
exit:
    free(dhcpv4_path);
    return rc;
}

static amxb_bus_ctx_t* dhcpv4_get_context(void) {
    if(NULL == dhcpv4_ctx) {
        dhcpv4_ctx = amxb_be_who_has("DHCPv4.");
    }
    return dhcpv4_ctx;
}

static amxb_bus_ctx_t* ip_get_context(void) {
    if(NULL == ip_ctx) {
        ip_ctx = amxb_be_who_has("IP.");
    }
    return ip_ctx;
}

static char* dhcpv4_add_client_instance(const char* name, const char* lower_layer) {
    char* path = NULL;
    amxc_var_t parameters;
    amxc_string_t alias;

    amxc_var_init(&parameters);
    amxc_string_init(&alias, 0);
    when_str_empty(name, exit);
    when_str_empty(lower_layer, exit);
    amxc_string_setf(&alias, "wanm-%s", name);

    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "Alias", amxc_string_get(&alias, 0));
    amxc_var_add_key(cstring_t, &parameters, "Interface", lower_layer);
    amxc_var_add_key(bool, &parameters, "Enable", false);

    path = component_add_instance("DHCPv4.Client.", &parameters, dhcpv4_get_context());
exit:
    amxc_var_clean(&parameters);
    amxc_string_clean(&alias);
    return path;
}

