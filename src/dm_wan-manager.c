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

#include "utils.h"
#include "dm_wan-manager.h"
#include "dm_wan_mode.h"

#include "integration/autosensing/autosensing.h"

#include "ctrl/mode_ctrl.h"

static wan_manager_app_t app;

static amxd_status_t is_valid_mode(const char* new_wan_mode);
static amxd_status_t add_default_intf_interface(amxd_object_t* root);
static amxb_bus_ctx_t* resolve_context(amxo_parser_t* parser);
static amxd_status_t wan_mode_set_mode(amxd_object_t* const object, const char* mode);
static bool interface_got_ip(const char* interface);

void _print_event(UNUSED const char* const sig_name,
                  UNUSED const amxc_var_t* const data,
                  UNUSED void* const priv) {
#ifdef TRACE_ON
    printf("event received - %s\n", sig_name);
    if(data != NULL) {
        printf("Event data = \n");
        fflush(stdout);
        amxc_var_dump(data, STDOUT_FILENO);
    }
#endif
}

amxd_dm_t* PRIVATE wan_get_dm(void) {
    return app.dm;
}

amxo_parser_t* PRIVATE wan_get_parser(void) {
    return app.parser;
}

amxb_bus_ctx_t* PRIVATE wan_get_context(void) {
    return app.context;
}

const char* PRIVATE wan_get_prefix(void) {
    amxc_var_t* setting = amxo_parser_get_config(wan_get_parser(), "prefix_");
    return amxc_var_constcast(cstring_t, setting);
}

int _wan_manager_main(int reason,
                      amxd_dm_t* dm,
                      amxo_parser_t* parser) {

    switch(reason) {
    case 0:
        app.dm = dm;
        app.parser = parser;
        app.context = resolve_context(parser);
        wan_mode_init();
        break;
    case 1:
        mode_ctrl_cleanup();
        wan_mode_cleanup();
        app.dm = NULL;
        app.parser = NULL;
        app.context = NULL;
        break;
    }

    return 0;
}

amxd_status_t _setWANMode(amxd_object_t* object,
                          UNUSED amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret) {
    amxd_status_t status = amxd_status_unknown_error;
    char* current_wan_mode_str = NULL;
    amxc_var_t* wan_mode = GET_ARG(args, "WANMode");
    amxc_var_t* autosensing = GET_ARG(args, "Autosensing");

    char* wan_mode_value = amxc_var_dyncast(cstring_t, wan_mode);
    bool autosensing_req = amxc_var_dyncast(bool, autosensing);
    SAH_TRACEZ_INFO(ME, "Configure mode %s Autosensing %d", wan_mode_value, autosensing_req);

    if(!autosensing_req) {
        char* current_mode = amxd_object_get_value(cstring_t, object, "OperationMode", NULL);
        if((NULL != current_mode) && (0 == strcmp("Automatic", current_mode))) {
            SAH_TRACEZ_INFO(ME, "Manual mode change requested. Switch to Manual opetation mode");
            wan_mode_set_mode(object, "Manual");
            autosensing_set_enable(false);
        }
        free(current_mode);
    }

    when_str_empty(wan_mode_value, exit);
    current_wan_mode_str = amxd_object_get_value(cstring_t, object, "WANMode", NULL);

    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, ret, "status", amxd_status_ok == wan_mode_set(wan_mode_value, current_wan_mode_str));

    status = amxd_status_ok;

exit:
    free(wan_mode_value);
    free(current_wan_mode_str);
    return status;
}

amxd_status_t _getCurrentWANModeStatus(UNUSED amxd_object_t* object,
                                       UNUSED amxd_function_t* func,
                                       UNUSED amxc_var_t* args,
                                       amxc_var_t* ret) {
    amxd_status_t status = amxd_status_ok;
    bool is_valid = wan_mode_is_valid();
    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, ret, "status", is_valid);

    return status;
}

amxd_status_t _getWANMode(UNUSED amxd_object_t* object,
                          UNUSED amxd_function_t* func,
                          UNUSED amxc_var_t* args,
                          UNUSED amxc_var_t* ret) {

    return amxd_status_function_not_implemented;
}


void _set_wan_mode(UNUSED const char* const event_name,
                   const amxc_var_t* const event_data,
                   UNUSED void* const priv) {

    const char* new_wan_mode = GETP_CHAR(event_data, "parameters.WANMode.to");
    const char* old_wan_mode = GETP_CHAR(event_data, "parameters.WANMode.from");
    SAH_TRACEZ_INFO(ME, "Change mode: [From = %s, To = %s]", old_wan_mode, new_wan_mode);
    when_null(new_wan_mode, exit);
    when_null(old_wan_mode, exit);
    when_true((0 == strcmp(new_wan_mode, old_wan_mode)), exit);

    if(amxd_status_ok == is_valid_mode(new_wan_mode)) {
        (void) wan_mode_set(new_wan_mode, old_wan_mode);
    } else {
        (void) wan_mode_dm_set(old_wan_mode);
    }

exit:
    return;
}

void _update_autosensing(UNUSED const char* const event_name,
                         const amxc_var_t* const event_data,
                         UNUSED void* const priv) {

    const char* new_wan_mode = GETP_CHAR(event_data, "parameters.OperationMode.to");
    const char* old_wan_mode = GETP_CHAR(event_data, "parameters.OperationMode.from");

    when_null(new_wan_mode, exit);
    when_null(old_wan_mode, exit);
    when_true((0 == strcmp(new_wan_mode, old_wan_mode)), exit);

    if(0 == strcmp(new_wan_mode, "Automatic")) {
        SAH_TRACEZ_INFO(ME, "WANManager set to automatic mode enable WANAutosensing");
        autosensing_set_enable(true);
    } else {
        SAH_TRACEZ_INFO(ME, "WANManager set to manual mode disable WANAutosensing");
        autosensing_set_enable(false);
    }


exit:
    return;
}

void _wan_mode_added(UNUSED const char* const event_name,
                     const amxc_var_t* const event_data,
                     UNUSED void* const priv) {
    const char* path = GETP_CHAR(event_data, "path");
    if(NULL != path) {
        amxd_object_t* root = amxd_dm_findf(wan_get_dm(), "%s", path);
        amxc_var_get_path(event_data, "object", AMXC_VAR_FLAG_DEFAULT);
        (void) add_default_intf_interface(root);
    }
}

amxd_status_t _interface_already_configured(amxd_object_t* object,
                                            UNUSED amxd_param_t* param,
                                            UNUSED amxd_action_t reason,
                                            const amxc_var_t* const args,
                                            UNUSED amxc_var_t* const retval,
                                            UNUSED void* priv) {
    amxd_status_t rc = amxd_status_invalid_value;
    amxd_object_t* root = amxd_object_get_parent(object);
    const char* value = amxc_var_constcast(cstring_t, args);

    when_null(root, exit);
    when_null(value, exit);
    rc = amxd_status_ok;

    amxd_object_for_each(instance, it, root) {
        amxd_object_t* obj = amxc_llist_it_get_data(it, amxd_object_t, it);
        amxc_var_t name;
        const char* child_name = NULL;
        amxc_var_init(&name);
        if(obj == object) {
            continue;
        }

        if(amxd_status_ok != amxd_object_get_param(obj, "Name", &name)) {
            rc = amxd_status_unknown_error;
            amxc_var_clean(&name);
            goto exit;
        }

        child_name = amxc_var_constcast(cstring_t, &name);

        if((NULL != child_name) && (0 == strcmp(value, child_name))) {
            rc = amxd_status_invalid_value;
            amxc_var_clean(&name);
            goto exit;
        }

        amxc_var_clean(&name);
    }

exit:
    return rc;
}

bool wan_mode_is_valid(void) {
    ipv4_mode_t mode = wan_mode_get_ipv4_mode();
    bool rc = false;
    SAH_TRACEZ_INFO(ME, "IPv4 mode of current WANMode %d", mode);

    switch(mode) {
    case IPv4_DHCP:
    {
        amxc_string_t* addresses_path = wan_mode_get_interface();
        rc = interface_got_ip(amxc_string_get(addresses_path, 0));
        amxc_string_delete(&addresses_path);
    }
    break;
    case IPv4_PPP:
    default:
        rc = false;
    }
    return rc;
}


static amxd_status_t is_valid_mode(const char* new_wan_mode) {
    return (NULL != new_wan_mode) && (NULL != get_wan_mode(new_wan_mode)) ? amxd_status_ok :  amxd_status_invalid_value;
}

static amxd_status_t add_default_intf_interface(amxd_object_t* root) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* instance = NULL;
    amxc_var_t parameters;

    when_null(root, exit);
    when_failed(amxc_var_init(&parameters), exit);
    when_failed(amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE), exit);
    when_null(amxc_var_add_key(cstring_t, &parameters, "Name", "data"), exit);

    when_false(((rc = amxd_object_add_instance(&instance, root, NULL, 0, &parameters)) != amxd_status_ok), exit);
    amxd_object_emit_add_inst(instance);

exit:
    amxc_var_clean(&parameters);
    return rc;
}

static amxb_bus_ctx_t* resolve_context(amxo_parser_t* parser) {
    amxo_connection_t* context = NULL;

    amxc_llist_iterate(it, parser->connections) {
        context = amxc_llist_it_get_data(it, amxo_connection_t, it);
        when_not_null(context, exit);
    }

exit:
    return NULL != context ? (amxb_bus_ctx_t*) context->priv : NULL;
}

static amxd_status_t wan_mode_set_mode(amxd_object_t* const object, const char* mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t status_parameter;

    when_null(object, exit);
    when_failed(amxc_var_init(&status_parameter), exit);
    when_failed(amxc_var_set_type(&status_parameter, AMXC_VAR_ID_CSTRING), exit);
    when_failed(amxc_var_set(cstring_t, &status_parameter, mode), exit);

    rc = amxd_object_set_param(object, "OperationMode", &status_parameter);

exit:
    amxc_var_clean(&status_parameter);
    return rc;
}

static bool interface_got_ip(const char* interface) {
    bool rc = false;
    amxc_string_t query_filter;
    amxc_var_t query;
    const amxc_llist_t* query_items = NULL;
    amxc_var_t* ipv4addresses = NULL;
    amxc_llist_it_t* first_set = NULL;
    int count = 0;
    amxc_var_init(&query);
    amxc_string_init(&query_filter, 0);

    when_str_empty(interface, exit);


    amxc_string_setf(&query_filter, "%s*", interface);
    SAH_TRACEZ_INFO(ME, "Check if IP.Interface path %s contain IPv4Addr objects", amxc_string_get(&query_filter, 0));
    when_false((AMXB_STATUS_OK == amxb_get(wan_get_context(), amxc_string_get(&query_filter, 0), 0, &query, 10)), exit);

    query_items = amxc_var_constcast(amxc_llist_t, &query);

    when_null(query_items, exit);
    if(1 < (count = amxc_llist_size(query_items))) {
        SAH_TRACEZ_INFO(ME, "NetDev query %s returned empty set %d", amxc_string_get(&query_filter, 0), count);
        goto exit;
    }
    first_set = amxc_llist_get_first(query_items);
    when_null_l(first_set, exit, "Unable to get first item from query list");
    ipv4addresses = amxc_container_of(first_set, amxc_var_t, lit);
    count = amxc_htable_size(amxc_var_constcast(amxc_htable_t, ipv4addresses));
    SAH_TRACEZ_INFO(ME, "IPv4Address set for %s contain %d elements", amxc_string_get(&query_filter, 0), count);
    rc = (0 != count);

exit:
    amxc_string_clean(&query_filter);
    amxc_var_clean(&query);

    return rc;
}
