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
#include "ctrl/mode_ctrl.h"
#include "integration/uci/uci.h"
#include "integration/uci/uci_ctrl.h"


#ifdef ME
#undef ME
#define ME "uci-ctrl"
#endif

typedef amxd_status_t (* convert_dm_to_uci_parameters)(amxc_var_t* parameters,
                                                       const amxc_var_t* const object,
                                                       const char* interface);

static amxd_status_t convert_dhpc4_to_uci(amxc_var_t* parameters, const amxc_var_t* const object, const char* interface);
static amxd_status_t convert_ppp4_to_uci(amxc_var_t* parameters, const amxc_var_t* const object, const char* interface);
static amxd_status_t convert_none_to_uci(amxc_var_t* parameters, const amxc_var_t* const object, const char* interface);


static convert_dm_to_uci_parameters converters[IPv4_Nr_] = {
    [IPv4_DHCP] = convert_dhpc4_to_uci,
    [IPv4_PPP] = convert_ppp4_to_uci,
    [IPv4_None] = convert_none_to_uci
};


amxd_status_t uci_remove_section_if_present(const char* section) {
    amxc_var_t ret;
    amxd_status_t rc = amxd_status_unknown_error;

    amxc_var_init(&ret);
    when_str_empty(section, exit);
    rc = amxd_status_ok;
    if(amxd_status_ok == uci_call("get", "network", section, NULL, NULL, &ret)) {
        amxc_var_t* wan = GETP_ARG(&ret, "0.values");
        amxc_var_log(&ret);

        if(NULL == wan) {
            SAH_TRACEZ_INFO(ME, "%s interface configuration not present in UCI", section);
            goto exit;
        }
        SAH_TRACEZ_INFO(ME, "Remove %s section from network config", section);
        when_failed_l((rc = uci_call("delete", "network", section, "interface", NULL, NULL)),
                      exit, "Unable to delete section %s from network UCI config", section);
        rc = uci_call("commit", "network", section, "interface", NULL, NULL);
    }
exit:
    amxc_var_clean(&ret);
    return rc;
}

amxd_status_t uci_write_section(const char* section, const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;

    when_str_empty(section, exit);
    when_null(parameters, exit);

    when_failed_l((rc = uci_call("add", "network", section, "interface", parameters, NULL)),
                  exit, "Unable to add section %s from network UCI config", section);
    when_failed_l((rc = uci_call("commit", "network", section, "interface", NULL, NULL)),
                  exit,
                  "Unable to commit changes to network configuration");
    SAH_TRACEZ_INFO(ME, "UCI network configuration updated");

exit:
    return rc;
}


amxc_var_t* uci_wan_mode_parameters(const amxc_var_t* const object, bool untagged) {
    amxc_var_t* parameters = NULL;
    const char* option = NULL;
    const char* interface = NULL;
    amxc_string_t wan_mode_interface;
    ipv4_mode_t mode = IPv4_None;
    amxc_string_init(&wan_mode_interface, 0);

    when_null(object, exit);

    interface = GETP_CHAR(object, "PhysicalInterface");

    if(untagged) {
        amxc_string_set(&wan_mode_interface, interface);
    } else {
        int vlan_id = GETP_UINT32(object, "VlanID");
        amxc_string_setf(&wan_mode_interface, "%s.%d", interface, vlan_id);
        SAH_TRACEZ_INFO(ME, "Configure tagged WAN mode: [interface=%s,VLAN_ID=%d]", interface, vlan_id);
    }

    amxc_var_new(&parameters);
    amxc_var_set_type(parameters, AMXC_VAR_ID_HTABLE);

    option = GETP_CHAR(object, "IPv4Mode");

    when_str_empty(option, exit);
    mode = wan_mode_ipv4_mode_from_str(option);
    when_true((IPv4_None == mode), error);
    when_failed(converters[mode](parameters, object, amxc_string_get(&wan_mode_interface, 0)), error);

exit:
    amxc_string_clean(&wan_mode_interface);
    return parameters;

error:
    amxc_string_clean(&wan_mode_interface);
    amxc_var_delete(&parameters);
    return NULL;
}


static amxd_status_t convert_dhpc4_to_uci(amxc_var_t* parameters, const amxc_var_t* const object, const char* interface) {
    amxd_status_t rc = amxd_status_unknown_error;

    when_null(parameters, exit);
    when_null(object, exit);
    rc = amxd_status_ok;
    amxc_var_add_key(cstring_t, parameters, "proto", "dhcp");
    amxc_var_add_key(cstring_t, parameters, "ifname", interface);

exit:
    return rc;
}

static amxd_status_t convert_ppp4_to_uci(amxc_var_t* parameters, const amxc_var_t* const object, const char* interface) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* username = NULL;
    const char* password = NULL;
    when_null(parameters, exit);
    when_null(object, exit);

    username = GETP_CHAR(object, "UserName");
    password = GETP_CHAR(object, "Password");

    when_str_empty(username, exit);
    when_str_empty(password, exit);

    rc = amxd_status_ok;
    SAH_TRACEZ_INFO(ME, "PPPoE credentials:[username=%s,password=%s]", username, password);

    amxc_var_add_key(cstring_t, parameters, "proto", "pppoe");
    amxc_var_add_key(cstring_t, parameters, "ifname", interface);
    amxc_var_add_key(cstring_t, parameters, "username", username);
    amxc_var_add_key(cstring_t, parameters, "password", password);
    amxc_var_add_key(cstring_t, parameters, "ipv6", "auto");
    amxc_var_add_key(cstring_t, parameters, "demand", "0");
    amxc_var_add_key(cstring_t, parameters, "keepalive", "0 86400");
    amxc_var_add_key(cstring_t, parameters, "mtu", "1492");
    amxc_var_add_key(cstring_t, parameters, "force_link", "1");

exit:
    return rc;
}


static amxd_status_t convert_none_to_uci(UNUSED amxc_var_t* parameters,
                                         UNUSED const amxc_var_t* const object,
                                         UNUSED const char* interface) {
    return amxd_status_invalid_action;
}



#undef ME