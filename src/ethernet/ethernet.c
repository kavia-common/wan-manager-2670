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
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_action.h>
#include <amxc/amxc_macros.h>
#include <amxb/amxb.h>

#include "ethernet/ethernet.h"
#include "wan_manager_utils.h"
#include "component.h"

#define ME "eth-ctrl"

static char* ethernet_add_vlan_instance(const char* lower_layer, uint32_t id, int32_t vlan_prio) {
    SAH_TRACEZ_IN(ME);
    char* path = NULL;
    amxc_string_t name;
    amxc_var_t parameters;
    amxc_var_t* tmp = NULL;

    amxc_string_init(&name, 0);
    amxc_string_setf(&name, "vlan%d", id);
    amxc_var_init(&parameters);
    when_str_empty(lower_layer, exit);

    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    tmp = amxc_var_add_new_key(&parameters, "Name");
    amxc_var_push(cstring_t, tmp, amxc_string_take_buffer(&name));
    amxc_var_add_key(cstring_t, &parameters, "Alias", GET_CHAR(tmp, NULL));
    amxc_var_add_key(cstring_t, &parameters, "LowerLayers", lower_layer);
    amxc_var_add_key(bool, &parameters, "Enable", false);
    amxc_var_add_key(uint32_t, &parameters, "VLANID", id);
    if(vlan_prio > -1) {
        amxc_var_add_key(uint32_t, &parameters, "VLANPriority", vlan_prio);
    }
    path = component_add_instance("Device.Ethernet.VLANTermination.", &parameters, ethernet_get_context());

exit:
    amxc_var_clean(&parameters);
    amxc_string_clean(&name);
    SAH_TRACEZ_OUT(ME);
    return path;
}

amxd_status_t ethernet_vlan_set_enable(const amxc_var_t* const parameters, const char* lower_layer, uint32_t vlan_id, int32_t vlan_prio, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t str_search;
    amxc_var_t set_params;
    char* vlan_path = NULL;

    amxc_var_init(&set_params);
    amxc_string_init(&str_search, 0);
    when_str_empty_trace(lower_layer, exit, ERROR, "Missing or empty LowerLayer");

    amxc_string_setf(&str_search, "Device.Ethernet.VLANTermination." \
                     "[VLANID==%d && LowerLayers=='%s'].", vlan_id, lower_layer);
    vlan_path = component_get_path_instance(ethernet_get_context(), amxc_string_get(&str_search, 0));

    if((NULL == vlan_path) && enable) {
        SAH_TRACEZ_INFO(ME, "VLAN Configuration not present, creating new vlan '%d' on '%s'",
                        vlan_id, lower_layer);

        vlan_path = ethernet_add_vlan_instance(lower_layer, vlan_id, vlan_prio);
        when_null_trace(vlan_path, exit, ERROR,
                        "Cannot create VLAN configuration for id %d with lowerlayer %s",
                        vlan_id, lower_layer);
    }
    SAH_TRACEZ_INFO(ME, "vlan_path %s, lower_layer %s", vlan_path, lower_layer);

    amxc_var_set_type(&set_params, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, &set_params, "Enable", enable);
    if(vlan_prio > -1) {
        amxc_var_add_key(uint32_t, &set_params, "VLANPriority", vlan_prio);
    }
    rc = component_set_params(vlan_path, ethernet_get_context(), &set_params);

    if(enable) {
        amxc_var_add_key(cstring_t, (amxc_var_t*) parameters, "VLANTermination", vlan_path);
    }

exit:
    free(vlan_path);
    amxc_var_clean(&set_params);
    amxc_string_clean(&str_search);
    SAH_TRACEZ_OUT(ME);
    return rc;
}
