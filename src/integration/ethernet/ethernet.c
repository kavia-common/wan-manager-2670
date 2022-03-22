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
#include <amxb/amxb.h>
#include <amxb/amxb_types.h>
#include <amxb/amxb_operators.h>
#include <amxb/amxb_be.h>
#include <stdlib.h>

#include "utils.h"
#include "integration/ethernet/ethernet.h"
#include "component.h"

#ifdef ME
#undef ME
#define ME "eth-ctrl"
#endif

static amxb_bus_ctx_t* context = NULL;
static amxb_bus_ctx_t* ethernet_get_context(void);
static amxc_string_t* ethernet_add_vlan_instance(const char* name, const char* lower_layer, uint32_t id);

static const char* vlan_query = "Ethernet.VLANTermination.*.";
static const char* ip_interface_query = "IP.Interface.*.";
static const char* ethernet = "Ethernet.";

amxd_status_t ethernet_vlan_set_enable(const amxc_var_t* const parameters, bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t* vlan_object = NULL;
    amxc_string_t* lower_layer = NULL;
    int vlan_id = -1;
    amxc_string_t interface;
    const char* ip_ref = NULL;

    amxc_string_init(&interface, 0);
    when_null(parameters, exit);
    vlan_id = GETP_UINT32(parameters, "VlanID");
    amxc_string_setf(&interface, "vlan%d", vlan_id);
    vlan_object = component_match_first_with_parameter_str("Alias",
                                                           amxc_string_get(&interface, 0),
                                                           vlan_query,
                                                           ethernet_get_context());
    if((NULL == vlan_object) && enable) {
        SAH_TRACEZ_INFO(ME, "VLAN Configuration not present add one for VLAN = %s", amxc_string_get(&interface, 0));
        ip_ref = GETP_CHAR(parameters, "IPReference");
        when_str_empty(ip_ref, exit);

        lower_layer = component_match_first_with_parameter_str("LowerLayers",
                                                               ip_ref,
                                                               ip_interface_query,
                                                               ethernet_get_context());
        when_null(lower_layer, exit);
        vlan_object = ethernet_add_vlan_instance(amxc_string_get(&interface, 0), amxc_string_get(lower_layer, 0), vlan_id);
        when_null_l(vlan_object,
                    exit,
                    "Cannot create VLAN configuration for id %d on base interface %s",
                    vlan_id,
                    ip_ref)

    }

    rc = component_set_enable(amxc_string_get(vlan_object, 0), ethernet_get_context(), enable);

exit:
    amxc_string_clean(&interface);
    amxc_string_delete(&vlan_object);
    amxc_string_delete(&lower_layer);
    return rc;
}

static amxb_bus_ctx_t* ethernet_get_context(void) {
    if(NULL == context) {
        context = amxb_be_who_has(ethernet);
    }
    return context;
}

static amxc_string_t* ethernet_add_vlan_instance(const char* name, const char* lower_layer, uint32_t id) {
    amxc_string_t* path = NULL;
    amxc_var_t parameters;

    amxc_var_init(&parameters);
    when_str_empty(name, exit);
    when_str_empty(lower_layer, exit);

    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "Name", name);
    amxc_var_add_key(cstring_t, &parameters, "Alias", name);
    amxc_var_add_key(cstring_t, &parameters, "LowerLayers", lower_layer);
    amxc_var_add_key(bool, &parameters, "Enable", false);
    amxc_var_add_key(uint32_t, &parameters, "VLANID", id);

    path = component_add_instance("Ethernet.VLANTermination.", &parameters, context);

exit:
    amxc_var_clean(&parameters);
    return path;
}

#undef ME