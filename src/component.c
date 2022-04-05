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
#include <stdlib.h>

#include "utils.h"
#include "component.h"

#ifdef ME
#undef ME
#define ME "com-ctrl"
#endif

static bool component_item_match(const char* parameter,
                                 const char* param_value,
                                 const amxc_htable_t* objects,
                                 amxc_string_t** path);

amxd_status_t component_set_enable(const char* component, amxb_bus_ctx_t* bus, bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t args;
    amxc_var_t parameters;
    amxc_var_t ret;

    amxc_var_init(&args);
    amxc_var_init(&parameters);
    amxc_var_init(&ret);

    when_null(bus, exit);
    when_str_empty(component, exit);

    SAH_TRACEZ_INFO(ME, "Set %s to %d", component, enable);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, &parameters, "Enable", enable);
    amxc_var_set_key(&args, "parameters", &parameters, AMXC_VAR_FLAG_COPY);

    if(AMXB_STATUS_OK != amxb_call(bus, component, "_set", &args, &ret, 5)) {
        SAH_TRACEZ_ERROR(ME, "%s client set enable %d failed", component, enable);
        goto exit;
    }
    rc = amxd_status_ok;

exit:
    amxc_var_clean(&args);
    amxc_var_clean(&parameters);
    amxc_var_clean(&ret);
    return rc;
}

amxc_string_t* component_match_first_with_parameter_str(const char* parameter,
                                                        const char* parameter_value,
                                                        const char* object_pattern,
                                                        amxb_bus_ctx_t* bus) {
    amxc_string_t* component_path = NULL;
    amxc_var_t query;
    const amxc_llist_t* query_items = NULL;

    amxc_var_init(&query);

    when_null(bus, exit);
    when_str_empty(parameter, exit);
    when_str_empty(parameter_value, exit);
    when_str_empty(object_pattern, exit);

    if(AMXB_STATUS_OK != amxb_get(bus, object_pattern, 0, &query, 10)) {
        SAH_TRACEZ_INFO(ME, "Objects not found for pattern %s", object_pattern);
        goto exit;
    }
    query_items = amxc_var_constcast(amxc_llist_t, &query);

    when_null(query_items, exit);
    amxc_llist_for_each(query_item, query_items) {
        amxc_var_t* client_item = amxc_llist_it_get_data(query_item, amxc_var_t, lit);
        if(component_item_match(parameter, parameter_value, amxc_var_constcast(amxc_htable_t, client_item), &component_path)) {
            goto exit;
        }
    }

exit:
    amxc_var_clean(&query);
    return component_path;
}

amxc_string_t* component_get_parameter_value(const char* parameter,
                                             const char* object,
                                             amxb_bus_ctx_t* bus) {
    amxc_var_t ret;
    amxc_string_t* parameter_value = NULL;
    amxc_var_t* item = NULL;
    amxc_string_t query_object;
    const char* value = NULL;

    amxc_var_init(&ret);
    amxc_string_init(&query_object, 0);

    when_null(bus, exit);
    when_str_empty(parameter, exit);
    when_str_empty(object, exit);

    amxc_string_setf(&query_object, "%s%s", object, parameter);

    if(AMXB_STATUS_OK != amxb_get(bus, amxc_string_get(&query_object, 0), 0, &ret, 5)) {
        SAH_TRACEZ_ERROR(ME, "Cannot fetch value for %s parameter", amxc_string_get(&query_object, 0));
        goto exit;
    }

    item = amxc_var_get_index(&ret, 0, AMXC_VAR_FLAG_DEFAULT);
    when_null(item, exit);
    item = amxc_var_get_index(item, 0, AMXC_VAR_FLAG_DEFAULT);
    when_null(item, exit);

    value = GETP_CHAR(item, parameter);
    amxc_string_new(&parameter_value, 0);
    amxc_string_set(parameter_value, value);

exit:
    amxc_string_clean(&query_object);
    amxc_var_clean(&ret);
    return parameter_value;

}

amxc_string_t* component_add_instance(const char* object_path,
                                      amxc_var_t* parameter,
                                      amxb_bus_ctx_t* bus) {
    amxc_string_t* object = NULL;
    amxc_var_t ret;
    amxc_var_t* new_instance = NULL;
    amxc_llist_it_t* object_item = NULL;
    amxc_var_init(&ret);

    when_str_empty(object_path, exit);
    when_null(bus, exit);

    when_false(AMXB_STATUS_OK == amxb_add(bus, object_path, 0, NULL, parameter, &ret, 5), exit);

    object_item = amxc_llist_get_first(amxc_var_constcast(amxc_llist_t, &ret));
    when_null(object_item, exit);

    new_instance = amxc_container_of(object_item, amxc_var_t, lit);
    when_null(object_item, exit);

    amxc_string_new(&object, 0);
    amxc_string_set(object, GETP_CHAR(new_instance, "object"));
exit:
    amxc_var_clean(&ret);
    return object;
}

amxd_status_t component_set_str_param(const char* component, amxb_bus_ctx_t* bus, const char* param, const char* value) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t args;
    amxc_var_t parameters;
    amxc_var_t ret;

    amxc_var_init(&args);
    amxc_var_init(&parameters);
    amxc_var_init(&ret);

    when_null(bus, exit);
    when_str_empty(param, exit);
    when_str_empty(value, exit);
    when_str_empty(component, exit);

    SAH_TRACEZ_INFO(ME, "%s Set %s to %s", component, param, value);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, param, value);
    amxc_var_set_key(&args, "parameters", &parameters, AMXC_VAR_FLAG_COPY);

    if(AMXB_STATUS_OK != amxb_call(bus, component, "_set", &args, &ret, 5)) {
        SAH_TRACEZ_ERROR(ME, "%s client set param %s failed", component, param);
        goto exit;
    }
    rc = amxd_status_ok;

exit:
    amxc_var_clean(&args);
    amxc_var_clean(&parameters);
    amxc_var_clean(&ret);
    return rc;
}

static bool component_item_match(const char* parameter,
                                 const char* param_value,
                                 const amxc_htable_t* objects,
                                 amxc_string_t** path) {
    bool matched = false;
    when_null(objects, exit);

    amxc_htable_for_each(iter, objects) {
        amxc_var_t* params = amxc_container_of(iter, amxc_var_t, hit);
        const char* alias = NULL;
        if(NULL == params) {
            continue;
        }
        alias = GETP_CHAR(params, parameter);
        if((NULL != alias) && (0 == strcmp(param_value, alias))) {
            matched = true;
            amxc_string_new(path, 0);
            amxc_string_set(*path, amxc_htable_it_get_key(iter));
            goto exit;
        }
    }

exit:
    return matched;
}


#undef ME