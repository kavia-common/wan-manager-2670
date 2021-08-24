/****************************************************************************
**
** SPDX-License-Identifier: <LICENSE_IDENTIFIER>
**
** SPDX-FileCopyrightText: Copyright (c) <CURRENT_YEAR> SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>

#include <debug/sahtrace.h>

#include "utils.h"
#include "dm_wan-manager.h"
#include "dm_wan_mode.h"
#include "dm_wan_mode_intf.h"
#include "ctrl/restart.h"
#include "ctrl/netdev_ctrl.h"
#include "ctrl/ipmanager_ctrl.h"

typedef enum {
    WAN_Mode_Enabled = 0,
    WAN_Mode_Disabled,
    WAN_Mode_Error,
    WAN_Mode_Nr_
} wan_mode_status_t;

static const char* wan_mode_status_str[WAN_Mode_Nr_] = {
    "Enabled",
    "Disabled",
    "Error",
};

static amxd_object_t* wan_manager;

static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode);
static const char* wan_mode_status_to_str(wan_mode_status_t status);
static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status);

void wan_mode_init(void) {
    const char* prefix = wan_get_prefix();
    if(NULL != prefix) {
        wan_manager = amxd_dm_findf(wan_get_dm(), "%sWANManager", prefix);
    }
}

void wan_mode_cleanup(void) {
    wan_manager = NULL;
}

amxd_status_t wan_mode_dm_set(const char* value) {
    amxd_status_t rc = amxd_status_unknown_error;
    when_null(value, exit);

    rc = amxd_object_set_value(cstring_t, wan_manager, "WANMode", value);

exit:
    return rc;
}

bool wan_mode_set(const char* wan_mode, UNUSED const char* ppp_user, UNUSED const char* ppp_password) {
    bool rc = false;
    amxd_object_t* current_wan_mode = NULL;
    char* current_wan_mode_str = NULL;
    amxd_object_t* new_wan_mode = NULL;
    size_t len = 0;

    when_null(wan_manager, exit);

    current_wan_mode_str = amxd_object_get_value(cstring_t, wan_manager, "WANMode", NULL);
    when_null(current_wan_mode_str, exit);

    when_false_l((0 != strcmp(wan_mode, current_wan_mode_str)), exit, "%s WAN mode is already configured", wan_mode);
    current_wan_mode = get_wan_mode(current_wan_mode_str);
    len = strlen(current_wan_mode_str);

    when_true(((NULL == current_wan_mode) && (0 != len)), exit);

    new_wan_mode = get_wan_mode(wan_mode);
    when_null_l(new_wan_mode, exit, "%s is not a valid WAN mode", wan_mode);

    if(NULL != current_wan_mode) {
        when_false_l(wan_mode_disable(current_wan_mode), exit, "WAN mode disable error [WANMode=%s]",
                     current_wan_mode_str);
    }

    wan_mode_dm_set(wan_mode);
    when_false_l(wan_mode_enable(new_wan_mode), exit, "WAN mode enable error [WANMode=%s]", wan_mode);
    rc = true;

    if(wan_mode_different_physical_type(current_wan_mode, new_wan_mode)) {
        rc = (amxd_status_ok == restart());
    }
exit:
    free(current_wan_mode_str);
    return rc;
}

bool wan_mode_disable(amxd_object_t* wan_mode) {
    bool rc = true;
    when_null(wan_mode, exit);
    (void) wan_mode_set_status(wan_mode, WAN_Mode_Disabled);
    when_failed_l(netdev_ctrl_unsubscribe(wan_mode), exit, "NetDev Unsubscribe error");
    when_failed_l(ipmanager_ctrl_unsubscribe(wan_mode), exit, "IPManager Unsubscribe error");
    rc = (amxd_status_ok == wan_mode_intf_disable_all(wan_mode));
exit:
    return rc;
}

bool wan_mode_enable(amxd_object_t* wan_mode) {
    bool rc = true;
    when_null(wan_mode, exit);

    when_failed_l(netdev_ctrl_subscribe(wan_mode), exit, "NetDev Subscribe error");
    when_failed_l(ipmanager_ctrl_subscribe(wan_mode), exit, "IPManager Subscribe error");
    rc = (amxd_status_ok == wan_mode_intf_enable_all(wan_mode));
    (void) wan_mode_set_status(wan_mode, (rc ? WAN_Mode_Enabled : WAN_Mode_Error));

exit:
    return rc;
}

amxd_object_t* get_wan_mode(const char* alias) {
    amxd_object_t* mode = NULL;
    amxd_object_t* wan_modes = amxd_object_get_child(wan_manager, "WAN");

    when_null(wan_modes, exit);
    when_null(alias, exit);

    amxc_llist_for_each(iter, &wan_modes->instances) {
        amxd_object_t* instance = amxc_llist_it_get_data(iter, amxd_object_t, it);
        char* instance_alias = NULL;
        bool found = false;

        when_null(instance, exit);
        instance_alias = amxd_object_get_value(cstring_t, instance, "Alias", NULL);
        when_null(instance_alias, exit);
        found = (0 == strcmp(alias, instance_alias));
        free(instance_alias);

        if(found) {
            mode = instance;
            goto exit;
        }
    }

exit:
    return mode;
}

static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode) {
    bool rc = true;
    amxc_var_t physical_type_current;
    amxc_var_t physical_type_new;
    int result = -1;

    amxc_var_init(&physical_type_current);
    amxc_var_init(&physical_type_new);

    when_null(current, exit);
    when_null(new_mode, exit);

    when_failed(amxd_object_get_param(current, "PhysicalType", &physical_type_current), exit);
    when_failed(amxd_object_get_param(new_mode, "PhysicalType", &physical_type_new), exit);
    when_failed(amxc_var_compare(&physical_type_current, &physical_type_new, &result), exit);

    rc = (0 != result);

exit:
    amxc_var_clean(&physical_type_new);
    amxc_var_clean(&physical_type_current);
    return rc;
}

static const char* wan_mode_status_to_str(wan_mode_status_t status) {
    return ((WAN_Mode_Enabled <= status) && (WAN_Mode_Nr_ > status))
           ? wan_mode_status_str[status] : wan_mode_status_str[WAN_Mode_Error];
}

static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t status_parameter;

    when_null(object, exit);
    when_failed(amxc_var_init(&status_parameter), exit);
    when_failed(amxc_var_set_type(&status_parameter, AMXC_VAR_ID_CSTRING), exit);
    when_failed(amxc_var_set(cstring_t, &status_parameter, wan_mode_status_to_str(status)), exit);

    rc = amxd_object_set_param(object, "Status", &status_parameter);

exit:
    amxc_var_clean(&status_parameter);
    return rc;
}