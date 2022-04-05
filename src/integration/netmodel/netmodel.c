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

#include <netmodel/common_api.h>
#include <netmodel/client.h>

#include "utils.h"
#include "integration/netmodel/netmodel.h"
#include "component.h"

#ifdef ME
#undef ME
#define ME "netmod-ctrl"
#endif

static amxc_string_t* netmodel_get_wan_interface(void);
static amxc_string_t* netmodel_get_flagged_interfaces(const char* intf, const char* flags);

amxc_string_t* netmodel_get_lower_layer_for_query(const char* flags) {
    amxc_string_t* wan_interface = NULL;
    amxc_string_t* flagged_interface = NULL;
    amxc_string_t* lower_layer = NULL;
    amxc_string_t current_wan_interface;
    amxc_var_t* active_interface = NULL;

    amxc_string_init(&current_wan_interface, 0);
    when_null(flags, exit);

    wan_interface = netmodel_get_wan_interface();
    when_null(wan_interface, exit);
    SAH_TRACEZ_INFO(ME, "WAN mode interface %s", amxc_string_get(wan_interface, 0));

    flagged_interface = netmodel_get_flagged_interfaces(amxc_string_get(wan_interface, 0), flags);
    when_null(flagged_interface, exit);
    SAH_TRACEZ_INFO(ME, "WAN mode flagged interface %s", amxc_string_get(flagged_interface, 0));

    amxc_string_setf(&current_wan_interface, "NetModel.Intf.%s.", amxc_string_get(flagged_interface, 0));
    lower_layer = component_get_parameter_value("InterfacePath",
                                                amxc_string_get(&current_wan_interface, 0),
                                                netmodel_get_amxb_bus());

    if(NULL != lower_layer) {
        SAH_TRACEZ_INFO(ME, "LowerLayer interface for query %s is %s", flags, amxc_string_get(lower_layer, 0));
    }

exit:
    amxc_var_delete(&active_interface);
    amxc_string_clean(&current_wan_interface);
    amxc_string_delete(&wan_interface);
    amxc_string_delete(&flagged_interface);
    return lower_layer;
}

static amxc_string_t* netmodel_get_wan_interface(void) {
    amxc_string_t* interface = NULL;
    amxc_var_t* wan_interface = NULL;
    const char* intf_name = NULL;

    wan_interface = netmodel_getIntfs("NetModel.Intf.resolver.", "eth_intf && upstream", netmodel_traverse_all);
    when_null(wan_interface, exit);
    intf_name = amxc_var_constcast(cstring_t, amxc_var_get_first(wan_interface));
    when_null_l(intf_name, exit, "NetModel WAN interface fetch error");

    amxc_string_new(&interface, 0);
    amxc_string_set(interface, intf_name);

exit:
    amxc_var_delete(&wan_interface);
    return interface;
}

static amxc_string_t* netmodel_get_flagged_interfaces(const char* intf, const char* flags) {
    amxc_string_t* interface = NULL;
    amxc_var_t* flagged_interfaces = NULL;
    const char* intf_name = NULL;
    amxc_string_t netmodel_intf;

    amxc_string_init(&netmodel_intf, 0);
    amxc_string_setf(&netmodel_intf, "NetModel.Intf.%s.", intf);

    flagged_interfaces = netmodel_getIntfs(amxc_string_get(&netmodel_intf, 0), flags, netmodel_traverse_up);
    when_null(flagged_interfaces, exit);

    intf_name = amxc_var_constcast(cstring_t, amxc_var_get_first(flagged_interfaces));
    when_null_l(intf_name, exit, "NetModel WAN interface fetch error");

    amxc_string_new(&interface, 0);
    amxc_string_set(interface, intf_name);

exit:
    amxc_string_clean(&netmodel_intf);
    amxc_var_delete(&flagged_interfaces);
    return interface;
}

#undef ME