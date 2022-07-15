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

#define ME "com-ctrl"

amxd_status_t component_set_enable(const char* component, amxb_bus_ctx_t* bus, bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t args;
    amxc_var_t parameters;
    amxc_var_t ret;

    amxc_var_init(&args);
    amxc_var_init(&parameters);
    amxc_var_init(&ret);

    when_null_trace(bus, exit, ERROR, "amxb_bus_ctx_t was empty");
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
    const char* value = NULL;
    char* path = NULL;
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_str_empty(object_path, exit);
    when_null(bus, exit);

    when_false(AMXB_STATUS_OK == amxb_add(bus, object_path, 0, NULL, parameter, &ret, 5), exit);
    value = GETP_CHAR(&ret, "0.path");
    if(value != NULL) {
        path = strdup(value);
    }
exit:
    amxc_var_clean(&ret);
    return path;
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
    when_null(value, exit);
    when_str_empty(component, exit);

    SAH_TRACEZ_INFO(ME, "'%s%s' = '%s'", component, param, value);
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
