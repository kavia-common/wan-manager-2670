/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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
#include <stdbool.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxm/amxm.h>

#include <netmodel/client.h>

#include "ctrl/mode_ctrl.h"
#include "dm_wan-manager.h"
#include "dm_wan_mode.h"

#include "autosensing/autosensing.h"
#include "netmodel/nm_query.h"

#define ME "wan-man"

static wan_manager_app_t app;

void _print_event(const char* const sig_name,
                  const amxc_var_t* const data,
                  UNUSED void* const priv) {
    (void) sig_name; // this argument can not be set to unused as it is used when tracing is enabled
    SAH_TRACEZ_IN(ME);
    SAH_TRACE_INFO("event received - %s", sig_name);
    SAH_TRACE_INFO("Event data = ");
    amxc_var_log(data);
    SAH_TRACEZ_OUT(ME);
}

amxd_dm_t* PRIVATE wan_get_dm(void) {
    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_OUT(ME);
    return app.dm;
}

amxo_parser_t* PRIVATE wan_get_parser(void) {
    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_OUT(ME);
    return app.parser;
}

int _wan_manager_main(int reason,
                      amxd_dm_t* dm,
                      amxo_parser_t* parser) {
    SAH_TRACEZ_IN(ME);
    switch(reason) {
    case 0:
        app.dm = dm;
        app.parser = parser;
        netmodel_initialize();
        wan_mode_init();
        autosensing_init();
        break;
    case 1:
        nm_query_ll_cleanup();
        stop_sensing_queries();
        stop_ra_queries();
        netmodel_cleanup();
        amxm_close_all();
        wan_mode_cleanup();
        app.dm = NULL;
        app.parser = NULL;
        break;
    }

    SAH_TRACEZ_OUT(ME);
    return 0;
}

amxd_status_t _setWANMode(amxd_object_t* object,
                          UNUSED amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_invalid_attr;
    const char* wan_mode_value = GET_CHAR(args, "WANMode");
    bool autosensing_req = GET_BOOL(args, "Autosensing");

    if(!autosensing_req) {
        char* current_mode = amxd_object_get_value(cstring_t, object, "OperationMode", NULL);
        if((NULL != current_mode) && (strcmp("Automatic", current_mode) == 0)) {
            mod_autosensing_stop();
        }
        SAH_TRACEZ_INFO(ME, "Configure %s as WANMode and set OperationMode to Manual", wan_mode_value);
        free(current_mode);
    } else {
        SAH_TRACEZ_INFO(ME, "Setting OperationMode to Automatic, requested WANMode will be ignored");
        wan_mode_value = NULL;
    }

    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);
    status = wan_mode_dm_set(wan_mode_value, autosensing_req ? "Automatic" : "Manual");
    amxc_var_add_key(bool, ret, "status", status == amxd_status_ok);

    SAH_TRACEZ_OUT(ME);
    return status;
}

static amxd_status_t set_intf_ip_mode(amxd_object_t* interface, const char* ip_mode_value, bool ipv4) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null_trace(interface, exit, ERROR, "object should not be NULL, can not set ip mode in datamodel");

    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_select_object(&trans, interface);

    amxd_trans_set_value(cstring_t, &trans, ipv4 ? "IPv4Mode" : "IPv6Mode", ip_mode_value);

    rc = amxd_trans_apply(&trans, wan_get_dm());

exit:
    amxd_trans_clean(&trans);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t _setIPv4Mode(amxd_object_t* object,
                           UNUSED amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_invalid_attr;
    const char* ip_mode_value = GET_CHAR(args, "IPv4Mode");
    const char* intf_alias = GET_CHAR(args, "InterfaceAlias");
    char* wan_mode = amxd_object_get_value(cstring_t, object, "WANMode", NULL);
    amxd_object_t* wan_mode_obj = NULL;
    amxd_object_t* interface = NULL;

    wan_mode_obj = get_wan_mode(wan_mode);
    when_null_trace(wan_mode_obj, exit, ERROR, "Failed to get the WANMode object");

    interface = amxd_object_findf(wan_mode_obj, ".Intf.[Alias == '%s'].", intf_alias);
    status = set_intf_ip_mode(interface, ip_mode_value, true);

exit:
    free(wan_mode);
    amxc_var_add_key(bool, ret, "status", status == amxd_status_ok);
    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _setIPv6Mode(amxd_object_t* object,
                           UNUSED amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_invalid_attr;
    const char* ip_mode_value = GET_CHAR(args, "IPv6Mode");
    const char* intf_alias = GET_CHAR(args, "InterfaceAlias");
    char* wan_mode = amxd_object_get_value(cstring_t, object, "WANMode", NULL);
    amxd_object_t* wan_mode_obj = NULL;
    amxd_object_t* interface = NULL;

    wan_mode_obj = get_wan_mode(wan_mode);
    when_null_trace(wan_mode_obj, exit, ERROR, "Failed to get the WANMode object");

    interface = amxd_object_findf(wan_mode_obj, ".Intf.[Alias == '%s'].", intf_alias);
    status = set_intf_ip_mode(interface, ip_mode_value, false);

exit:
    free(wan_mode);
    amxc_var_add_key(bool, ret, "status", status == amxd_status_ok);
    SAH_TRACEZ_OUT(ME);
    return status;
}

void _Reset(UNUSED amxd_object_t* object,
            UNUSED amxd_function_t* func,
            UNUSED amxc_var_t* args,
            UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    char* current_wan_mode_str = get_current_wan_mode_str();

    rv = wan_mode_set(current_wan_mode_str, current_wan_mode_str);
    when_failed_trace(rv, exit, ERROR, "Failed to reset wan mode '%s'", current_wan_mode_str);

exit:
    free(current_wan_mode_str);
    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _getCurrentWANModeStatus(UNUSED amxd_object_t* object,
                                       UNUSED amxd_function_t* func,
                                       UNUSED amxc_var_t* args,
                                       amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_unknown_error;
    amxd_object_t* current_mode_obj = get_current_wan_mode();
    bool mode_active = false;
    char* ip_reference = NULL;

    when_null_trace(current_mode_obj, exit, ERROR, " Failed to get the current wan mode object");

    amxd_object_for_each(instance, it, amxd_object_findf(current_mode_obj, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        ip_reference = amxd_object_get_value(cstring_t, interface, "IPv4Reference", NULL);
        when_str_empty(ip_reference, exit);     // When the ip_reference is empty the mode is not active
        mode_active = netmodel_isUp(ip_reference, "ipv4-up", netmodel_traverse_this);
        break;
    }

    rv = amxd_status_ok;

exit:
    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, ret, "active", mode_active);
    free(ip_reference);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _getWANMode(amxd_object_t* object,
                          UNUSED amxd_function_t* func,
                          UNUSED amxc_var_t* args,
                          amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_unknown_error;
    amxc_var_t* wan_mode_config = NULL;
    amxc_var_t* interface_config = NULL;
    amxd_object_t* wan_obj = NULL;

    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);

    rv = amxd_object_get_params(object, ret, amxd_dm_access_public);
    when_failed_trace(rv, exit, ERROR, "Failed to fetch general configuration");

    wan_mode_config = amxc_var_add_key(amxc_htable_t, ret, "WANModeConfig", NULL);
    wan_obj = get_wan_mode(GET_CHAR(ret, "WANMode"));
    rv = amxd_object_get_params(wan_obj, wan_mode_config, amxd_dm_access_public);
    when_failed_trace(rv, exit, ERROR, "Failed to fetch current wan mode configuration");

    interface_config = amxc_var_add_key(amxc_htable_t, wan_mode_config, "Interfaces", NULL);
    amxd_object_iterate(instance, it, amxd_object_findf(wan_obj, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        amxc_var_t* intf_params = NULL;
        amxc_var_t* address_config = NULL;

        when_null_trace(interface, exit, ERROR, "No interface object found");
        intf_params = amxc_var_add_key(amxc_htable_t, interface_config, interface->name, NULL);
        rv = amxd_object_get_params(interface, intf_params, amxd_dm_access_public);
        when_failed_trace(rv, exit, ERROR, "Failed to fetch current interface configuration");

        address_config = amxc_var_add_key(amxc_htable_t, intf_params, "IPv4Addresses", NULL);
        amxd_object_iterate(instance, ip_it, amxd_object_findf(interface, ".IPv4Address.")) {
            amxd_object_t* ip_addr_obj = amxc_container_of(ip_it, amxd_object_t, it);
            amxc_var_t* addr_params = NULL;

            when_null_trace(ip_addr_obj, exit, ERROR, "No IPv4Address object found");
            addr_params = amxc_var_add_key(amxc_htable_t, address_config, ip_addr_obj->name, NULL);
            rv = amxd_object_get_params(ip_addr_obj, addr_params, amxd_dm_access_public);
            when_failed_trace(rv, exit, ERROR, "Failed to fetch current IPv4Address configuration");
        }

        address_config = amxc_var_add_key(amxc_htable_t, intf_params, "IPv6Addresses", NULL);
        amxd_object_iterate(instance, ip_it, amxd_object_findf(interface, ".IPv6Address.")) {
            amxd_object_t* ip_addr_obj = amxc_container_of(ip_it, amxd_object_t, it);
            amxc_var_t* addr_params = NULL;

            when_null_trace(ip_addr_obj, exit, ERROR, "No IPv6Address object found");
            addr_params = amxc_var_add_key(amxc_htable_t, address_config, ip_addr_obj->name, NULL);
            rv = amxd_object_get_params(ip_addr_obj, addr_params, amxd_dm_access_public);
            when_failed_trace(rv, exit, ERROR, "Failed to fetch current IPv6Address configuration");
        }
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

void _set_wan_mode(UNUSED const char* const event_name,
                   const amxc_var_t* const event_data,
                   UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    const char* new_wan_mode = GETP_CHAR(event_data, "parameters.WANMode.to");
    const char* old_wan_mode = GETP_CHAR(event_data, "parameters.WANMode.from");
    char* current_operation_mode = amxd_object_get_value(cstring_t, get_wan_manager_obj(), "OperationMode", NULL);

    when_str_empty_trace(current_operation_mode, exit, ERROR, "Could not get current operation mode");
    when_true_trace(strcmp(current_operation_mode, "Automatic") == 0, exit, WARNING, "Ignoring changes made when autosensing is active");

    if(is_valid_mode(new_wan_mode) == amxd_status_ok) {
        wan_mode_set(new_wan_mode, old_wan_mode);
    } else {
        wan_mode_dm_set(old_wan_mode, NULL);
    }

exit:
    free(current_operation_mode);
    SAH_TRACEZ_OUT(ME);
    return;
}

amxd_status_t _interface_already_configured(amxd_object_t* object,
                                            UNUSED amxd_param_t* param,
                                            UNUSED amxd_action_t reason,
                                            const amxc_var_t* const args,
                                            UNUSED amxc_var_t* const retval,
                                            UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_invalid_value;
    amxd_object_t* root = amxd_object_get_parent(object);
    const char* value = GET_CHAR(args, NULL);

    when_null(root, exit);
    when_null(value, exit);

    amxd_object_for_each(instance, it, root) {
        amxd_object_t* obj = amxc_llist_it_get_data(it, amxd_object_t, it);
        const amxc_var_t* name = NULL;
        const char* child_name = NULL;
        if(obj == object) {
            continue;
        }

        name = amxd_object_get_param_value(obj, "Name");
        when_null_status(name, exit, rc = amxd_status_unknown_error);
        child_name = GET_CHAR(name, NULL);
        when_true((NULL != child_name) && (strcmp(value, child_name) == 0), exit);

    }
    rc = amxd_status_ok;

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t is_valid_mode(const char* new_wan_mode) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_invalid_attr;

    if(get_wan_mode(new_wan_mode) != NULL) {
        rc = amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return rc;
}

static void dm_wan_manager_change_physical(const amxc_var_t* const event_data,
                                           const char* query) {
    SAH_TRACEZ_IN(ME);
    const char* type = NULL;
    when_null_trace(event_data, exit, ERROR, "No data");
    type = GETP_CHAR(event_data, query);
    SAH_TRACEZ_INFO(ME, "PhysicalType: %s", type);
    nm_query_ll_add(type);
exit:
    SAH_TRACEZ_OUT(ME);
    return;

}

void _dm_wan_manager_physical_type_changed(UNUSED const char* const event_name,
                                           const amxc_var_t* const event_data,
                                           UNUSED void* const priv) {

    SAH_TRACEZ_IN(ME);
    dm_wan_manager_change_physical(event_data, "parameters.PhysicalType.to");
    SAH_TRACEZ_OUT(ME);
}

void _dm_wan_manager_wan_added(UNUSED const char* const event_name,
                               const amxc_var_t* const event_data,
                               UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    dm_wan_manager_change_physical(event_data, "parameters.PhysicalType");
    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _interface_destroy(amxd_object_t* intf,
                                 UNUSED amxd_param_t* param,
                                 amxd_action_t reason,
                                 UNUSED const amxc_var_t* const args,
                                 UNUSED amxc_var_t* const retval,
                                 UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_unknown_error;
    netmodel_query_t* query = NULL;

    when_false_trace(reason == action_object_destroy, exit, NOTICE, "Wrong reason, expected action_object_destroy(%d) got %d", action_object_destroy, reason);
    when_null_trace(intf, exit, ERROR, "Interface can not be NULL");
    when_false_status(intf->type == amxd_object_instance, exit, rv = amxd_status_ok);

    query = (netmodel_query_t*) intf->priv;
    netmodel_closeQuery(query);
    intf->priv = NULL;
    rv = amxd_status_ok;

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}