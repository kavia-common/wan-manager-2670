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
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>
#include <amxc/amxc_macros.h>
#include <amxb/amxb.h>
#include <amxb/amxb_types.h>
#include <amxb/amxb_operators.h>

#include "component.h"
#include "wan_manager_utils.h"

#define ME "com-ctrl"

amxd_status_t component_set_bool(const char* component, amxb_bus_ctx_t* bus, const char* param, bool value) {
    int rc = -1;
    amxc_var_t parameters;

    amxc_var_init(&parameters);

    when_null_trace(bus, exit, ERROR, "amxb_bus_ctx_t was empty");
    when_str_empty(component, exit);

    SAH_TRACEZ_INFO(ME, "Set %s.%s to %d", component, param, value);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, &parameters, param, value);

    rc = component_set_params(component, bus, &parameters);
    when_failed_trace(rc, exit, ERROR, "%s client set '%s' to '%d' failed with '%d'", component, param, value, rc);

exit:
    amxc_var_clean(&parameters);
    return (rc == 0) ? amxd_status_ok : amxd_status_unknown_error;
}

amxd_status_t component_set_enable(const char* component, amxb_bus_ctx_t* bus, bool enable) {
    return component_set_bool(component, bus, "Enable", enable);
}

/**
   @brief
   Retrieves the datamodel path of an instance

   @param bus context
   @param query for example "DHCPv4.Client.[Interface=='Device.IP.Interface.2.']."
   @return string on success
           NULL pointer if failed
 */
char* component_get_path_instance(amxb_bus_ctx_t* bus,
                                  const char* query) {
    amxc_var_t ret;
    const char* result = NULL;
    char* ret_str = NULL;

    amxc_var_init(&ret);
    amxb_get(bus, query, 0, &ret, 5);
    result = amxc_var_key(GETP_ARG(&ret, "0.0"));
    when_str_empty_trace(result, exit, INFO, "No results for '%s'", query);
    SAH_TRACEZ_INFO(ME, "%s returned %s", query, result);
    ret_str = strdup(result);
exit:
    amxc_var_clean(&ret);
    return ret_str;
}

char* component_add_instance(const char* object_path,
                             amxc_var_t* parameter,
                             amxb_bus_ctx_t* bus) {
    int rv = -1;
    const char* value = NULL;
    char* path = NULL;
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_str_empty(object_path, exit);
    when_null_trace(bus, exit, ERROR, "No bus context provided");

    rv = amxb_add(bus, object_path, 0, NULL, parameter, &ret, 5);
    when_failed_trace(rv, exit, ERROR, "Failed to add instance to '%s', error '%d'", object_path, rv);
    value = GETP_CHAR(&ret, "0.path");
    if(value != NULL) {
        path = strdup(value);
    }

exit:
    amxc_var_clean(&ret);
    return path;
}

amxd_status_t component_set_str_param(const char* component, amxb_bus_ctx_t* bus, const char* param, const char* value) {
    amxd_status_t rc = amxd_status_ok;
    amxc_var_t parameters;

    amxc_var_init(&parameters);

    when_null(bus, exit);
    when_str_empty(param, exit);
    when_null(value, exit);
    when_str_empty(component, exit);

    SAH_TRACEZ_INFO(ME, "'%s%s' = '%s'", component, param, value);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, param, value);

    rc = component_set_params(component, bus, &parameters);

exit:
    amxc_var_clean(&parameters);
    return rc;
}

amxd_status_t component_del_instance(const char* object_path,
                                     amxb_bus_ctx_t* bus) {
    amxc_var_t ret;
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_init(&ret);

    when_str_empty(object_path, exit);
    when_null(bus, exit);

    if(AMXB_STATUS_OK != amxb_del(bus, object_path, 0, NULL, &ret, 5)) {
        SAH_TRACEZ_ERROR(ME, "Could not delete instance %s", object_path);
        goto exit;
    }
    rc = amxd_status_ok;
exit:
    amxc_var_clean(&ret);
    return rc;
}

amxd_status_t component_get_param(amxc_var_t* ret_var, const char* component, amxb_bus_ctx_t* bus, const char* parameter) {
    amxc_string_t param_path;
    amxd_status_t rc = amxd_status_unknown_error;

    amxc_string_init(&param_path, 0);

    when_null(bus, exit);
    when_str_empty(component, exit);
    when_str_empty(parameter, exit);

    amxc_string_setf(&param_path, "%s.%s", component, parameter);

    if(AMXB_STATUS_OK != amxb_get(bus, amxc_string_get(&param_path, 0), 0, ret_var, 3)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get %s", amxc_string_get(&param_path, 0));
        goto exit;
    }
    rc = amxd_status_ok;
exit:
    amxc_string_clean(&param_path);
    return rc;
}


amxd_status_t component_add_string_to_csv(const char* component, amxb_bus_ctx_t* bus, const char* parameter, const char* str) {
    amxc_var_t orig_value;
    amxc_var_t new_value;
    amxc_var_t llist;
    amxd_status_t rc = amxd_status_unknown_error;

    amxc_var_init(&orig_value);
    amxc_var_init(&new_value);
    amxc_var_init(&llist);
    amxc_var_set_type(&llist, AMXC_VAR_ID_LIST);

    when_null(bus, exit);
    when_str_empty(component, exit);
    when_str_empty(parameter, exit);
    when_str_empty(str, exit);

    when_failed_trace(component_get_param(&orig_value, component, bus, parameter), exit, ERROR, "Failed to get %s.%s", component, parameter);

    if(!STRING_EMPTY(GETP_CHAR(&orig_value, "0.0.0"))) {
        when_failed_trace(amxc_var_convert(&llist, GETP_ARG(&orig_value, "0.0.0"), AMXC_VAR_ID_LIST), exit, ERROR, "Failed to cast %s.%s to list variant", component, parameter);
    }

    add_str_to_list(&llist, str);
    when_failed_trace(amxc_var_convert(&new_value, &llist, AMXC_VAR_ID_CSV_STRING), exit, ERROR, "Failed to cast list to CSV string");

    when_failed_trace(component_set_str_param(component, bus, parameter, GET_CHAR(&new_value, NULL)), exit, ERROR, "Failed to set %s.%s to %s", component, parameter, GET_CHAR(&new_value, NULL));

    rc = amxd_status_ok;

exit:
    amxc_var_clean(&new_value);
    amxc_var_clean(&orig_value);
    amxc_var_clean(&llist);
    return rc;
}

amxd_status_t component_remove_string_from_csv(const char* component, amxb_bus_ctx_t* bus, const char* parameter, const char* str) {
    amxc_var_t orig_value;
    amxc_var_t new_value;
    amxc_var_t llist;
    amxd_status_t rc = amxd_status_unknown_error;

    amxc_var_init(&orig_value);
    amxc_var_init(&new_value);
    amxc_var_init(&llist);
    amxc_var_set_type(&llist, AMXC_VAR_ID_LIST);

    when_null(bus, exit);
    when_str_empty(component, exit);
    when_str_empty(parameter, exit);
    when_str_empty(str, exit);

    when_failed_trace(component_get_param(&orig_value, component, bus, parameter), exit, ERROR, "Failed to get %s.%s", component, parameter);

    if(STRING_EMPTY(GETP_CHAR(&orig_value, "0.0.0"))) {
        SAH_TRACEZ_INFO(ME, "CSV is empty, can't remove %s from this", str);
        rc = amxd_status_ok;
        goto exit;
    }

    when_failed_trace(amxc_var_convert(&llist, GETP_ARG(&orig_value, "0.0.0"), AMXC_VAR_ID_LIST), exit, ERROR, "Failed to cast %s.%s to list variant", component, parameter);
    remove_str_from_list(&llist, str);
    when_failed_trace(amxc_var_convert(&new_value, &llist, AMXC_VAR_ID_CSV_STRING), exit, ERROR, "Failed to cast list to CSV string");
    when_failed_trace(component_set_str_param(component, bus, parameter, GET_CHAR(&new_value, NULL)), exit, ERROR, "Failed to set %s.%s to %s", component, parameter, GET_CHAR(&new_value, NULL));

    rc = amxd_status_ok;

exit:
    amxc_var_clean(&new_value);
    amxc_var_clean(&orig_value);
    amxc_var_clean(&llist);
    return rc;
}

amxd_status_t component_set_params(const char* component, amxb_bus_ctx_t* bus, amxc_var_t* values) {
    amxc_var_t ret;
    int rc = -1;

    when_null(bus, exit);
    when_null(values, exit);
    when_str_empty(component, exit);

    amxc_var_init(&ret);

    rc = amxb_set(bus, component, values, &ret, 10);
    when_failed_trace(rc, exit, ERROR, "%s client set params failed with error code %d", component, rc);

exit:
    amxc_var_clean(&ret);
    return (rc == 0) ? amxd_status_ok : amxd_status_unknown_error;
}