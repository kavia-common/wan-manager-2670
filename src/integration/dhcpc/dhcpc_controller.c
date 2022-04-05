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
#include <debug/sahtrace.h>
#include <stdio.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>
#include <amxc/amxc_macros.h>
#include <stdlib.h>

#include "utils.h"
#include "dm_wan_mode.h"
#include "dm_wan-manager.h"
#include "ctrl/mode_ctrl.h"
#include "integration/dhcpc/dhcpc.h"
#include "integration/ethernet/ethernet.h"
#include "integration/netmodel/netmodel.h"

#include "component.h"

#ifdef ME
#undef ME
#define ME "dhcpc-ctrl"
#endif

static const char* dhcp_client = "DHCPv4.Client";
static const char* dhcp_query = "DHCPv4.Client.*";
static mode_ctrl_actions_t dhcpc_actions;
static amxb_bus_ctx_t* context = NULL;

static amxd_status_t dhcpc_enable(wan_mode_type_t mode, const amxc_var_t* const parameters);
static amxd_status_t dhcpc_disable(wan_mode_type_t mode, const amxc_var_t* const parameters);
static amxb_bus_ctx_t* dhcpc_get_context(void);
static amxc_string_t* dhcp_add_v4_client_instance(const char* name, const char* lower_layer);
static amxd_status_t dhcp_set_ip_interface(const char* interface);

AMXB_CONSTRUCTOR static void dhcp_controller_init(void) {
    dhcpc_actions.enable = dhcpc_enable;
    dhcpc_actions.disable = dhcpc_disable;
    (void) register_mode_controller(Untagged_DHCP, &dhcpc_actions);
    (void) register_mode_controller(Tagged_DHCP, &dhcpc_actions);

}

static amxd_status_t dhcpc_enable(wan_mode_type_t mode, const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_alias = NULL;
    const char* object = NULL;
    const char* ip_reference = NULL;
    amxc_string_t* dhcpc_path = NULL;

    when_null(parameters, exit);

    if(Tagged_DHCP == mode) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
    }

    intf_alias = GETP_CHAR(parameters, "Alias");
    ip_reference = GETP_CHAR(parameters, "IPReference");

    when_str_empty(intf_alias, exit);
    when_str_empty(ip_reference, exit);

    SAH_TRACEZ_INFO(ME, "Enable DHCPv4  client configuration with Alias %s", intf_alias);
    dhcpc_path = component_match_first_with_parameter_str("Alias", intf_alias, dhcp_query, dhcpc_get_context());

    if(NULL == dhcpc_path) {
        SAH_TRACEZ_INFO(ME, "DHCPv4  %s Client configuration not exist. Create it", intf_alias);
        dhcpc_path = dhcp_add_v4_client_instance(intf_alias, ip_reference);
        when_null_l(dhcpc_path, exit, "Add DHCPv4 Client instance error");
    }

    if(Untagged_DHCP == mode) {
        when_failed(dhcp_set_ip_interface(ip_reference), exit);
    }

    object = amxc_string_get(dhcpc_path, 0);
    rc = component_set_enable(object, dhcpc_get_context(), true);

exit:
    amxc_string_delete(&dhcpc_path);
    return rc;
}

static amxd_status_t dhcpc_disable(wan_mode_type_t mode, const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* object = NULL;
    amxc_string_t dhcpc_path;
    amxc_string_init(&dhcpc_path, 0);

    when_null(parameters, exit);

    object = GETP_CHAR(parameters, "Alias");
    when_str_empty(object, exit);

    SAH_TRACEZ_INFO(ME, "Disable %s DHCPv4 client configuration", object);

    amxc_string_setf(&dhcpc_path, "DHCPv4.Client.%s", object);
    object = amxc_string_get(&dhcpc_path, 0);
    when_failed((rc = component_set_enable(object, dhcpc_get_context(), false)), exit);

    if(Tagged_DHCP == mode) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

exit:
    amxc_string_clean(&dhcpc_path);
    return rc;
}

static amxb_bus_ctx_t* dhcpc_get_context(void) {
    if(NULL == context) {
        context = amxb_be_who_has(dhcp_client);
    }
    return context;
}

static amxc_string_t* dhcp_add_v4_client_instance(const char* name, const char* lower_layer) {
    amxc_string_t* path = NULL;
    amxc_var_t parameters;

    amxc_var_init(&parameters);
    when_str_empty(name, exit);
    when_str_empty(lower_layer, exit);

    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "Alias", name);
    amxc_var_add_key(cstring_t, &parameters, "Interface", lower_layer);
    amxc_var_add_key(bool, &parameters, "Enable", false);

    path = component_add_instance("DHCPv4.Client.", &parameters, context);

exit:
    amxc_var_clean(&parameters);
    return path;
}

static amxd_status_t dhcp_set_ip_interface(const char* interface) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t* lower_layer = NULL;

    when_null(interface, exit);

    lower_layer = netmodel_get_lower_layer_for_query("eth_link");
    when_null(lower_layer, exit);
    rc = component_set_str_param(interface, dhcpc_get_context(), "LowerLayers", amxc_string_get(lower_layer, 0));
exit:
    amxc_string_delete(&lower_layer);
    return rc;
}

#undef ME