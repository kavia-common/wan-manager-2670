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
#include <amxm/amxm.h>

#include "ctrl/mode_ctrl.h"
#include "netmodel/nm_query.h"
#include "dm_wan_mode.h"
#include "dm_wan-manager.h"
#include "autosensing/autosensing.h"
#include "component.h"

#define ME "as-ctrl"

static int autosensing_set_wan_mode(UNUSED const char* function_name,
                                    amxc_var_t* args,
                                    UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    const char* next_wan_mode_str = GET_CHAR(args, "Alias");
    const char* current_wan_mode_str = get_current_wan_mode_str();

    if(((next_wan_mode_str) == NULL) || (*(next_wan_mode_str) == 0)) {
        SAH_TRACEZ_ERROR(ME, "Invalid mode provided, stop sensing");
        mod_autosensing_stop();
        goto exit;
    }
    SAH_TRACEZ_INFO(ME, "Setting mode '%s' for autosensing", next_wan_mode_str);

    rv = wan_mode_set(next_wan_mode_str, current_wan_mode_str);
    when_failed_trace(rv, exit, ERROR, "Failed to set wan mode '%s'", next_wan_mode_str);

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

static int autosensing_is_up_query_start(UNUSED const char* function_name,
                                         UNUSED amxc_var_t* args,
                                         UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;

    SAH_TRACEZ_INFO(ME, "Autosensing, start sensing query");
    rv = nm_query_mode_active();

    SAH_TRACEZ_OUT(ME);
    return rv;
}

static int autosensing_is_up_query_stop(UNUSED const char* function_name,
                                        UNUSED amxc_var_t* args,
                                        UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);

    SAH_TRACEZ_INFO(ME, "Autosensing, stop sensing query");
    nm_close_sensing_queries();

    SAH_TRACEZ_OUT(ME);
    return 0;
}

static int register_core_functions(void) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxm_shared_object_t* so = amxm_get_so("self");
    amxm_module_t* mod = NULL;

    when_null(so, exit);
    when_failed(amxm_module_register(&mod, so, MOD_DM_MNGR), exit);
    rv = amxm_module_add_function(mod, "set-mode", autosensing_set_wan_mode);
    when_failed_trace(rv, exit, ERROR, "Failed to register function set-mode");
    rv = amxm_module_add_function(mod, "isup-sensing-start", autosensing_is_up_query_start);
    when_failed_trace(rv, exit, ERROR, "Failed to register function isup-sensing-start");
    rv = amxm_module_add_function(mod, "isup-sensing-stop", autosensing_is_up_query_stop);
    when_failed_trace(rv, exit, ERROR, "Failed to register function isup-sensing-stop");

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

int autosensing_init(void) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxc_var_t lcontrollers;
    amxc_string_t mod_path;
    amxm_shared_object_t* so = NULL;
    amxd_object_t* wanm_obj = get_wan_manager_obj();
    amxo_parser_t* parser = wan_get_parser();
    const amxc_var_t* controllers = amxd_object_get_param_value(wanm_obj, "SupportedControllers");
    const char* const mod_dir = GET_CHAR(&parser->config, "external-mod-dir");

    amxc_var_init(&lcontrollers);
    amxc_string_init(&mod_path, 0);

    when_null_trace(wanm_obj, exit, ERROR, "Failed to get the WANManager object");
    when_null_trace(controllers, exit, ERROR, "Failed to get the supported controllers");

    amxc_var_convert(&lcontrollers, controllers, AMXC_VAR_ID_LIST);
    amxc_var_for_each(controller, &lcontrollers) {
        const char* name = GET_CHAR(controller, NULL);
        amxc_string_setf(&mod_path, "%s/%s.so", mod_dir, name);
        rv = amxm_so_open(&so, name, amxc_string_get(&mod_path, 0));
        SAH_TRACEZ_INFO(ME, "Loading controller '%s' %s", name, rv ? "failed" : "successful");
        when_failed(rv, exit);
    }

    register_core_functions();

    rv = 0;

exit:
    amxc_string_clean(&mod_path);
    amxc_var_clean(&lcontrollers);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Execute a function from the autosensing module.
 * @param function The module function to execute
 * @param data data variant containing the data to provide the module function
 * @return Will return 0 if successful and returns an error code when it fails
 */
static int mod_autosensing_execute_function(const char* function, amxc_var_t* data) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxc_var_t ret;
    amxd_object_t* wanm_obj = get_wan_manager_obj();
    const amxc_var_t* ctrlr = amxd_object_get_param_value(wanm_obj, "Controller");
    const char* ctrlr_name = GET_CHAR(ctrlr, NULL);

    amxc_var_init(&ret);
    when_str_empty(function, exit);
    when_str_empty_trace(ctrlr_name, exit, ERROR, "No controller name found");

    SAH_TRACEZ_INFO(ME, "%s -> call implementation (controller = '%s')", function, ctrlr_name);

    rv = amxm_execute_function(ctrlr_name, MOD_AUTOSENSING_CTRL, function, data, &ret);
    if(rv != 0) {
        SAH_TRACEZ_ERROR(ME, "function %s failed", function);
    }

exit:
    amxc_var_clean(&ret);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Call to autosensing module to start the sensing
 * @return Will return 0 if successful and returns an error code when it fails
 */
int mod_autosensing_start(void) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxc_var_t data;
    amxc_var_t* params = NULL;
    amxc_var_t* modes = NULL;
    amxd_object_t* wanm_obj = get_wan_manager_obj();
    amxd_object_t* wan_obj = amxd_object_findf(wanm_obj, "WAN.");
    bool sense_current_mode = false;

    when_null_trace(wanm_obj, exit, ERROR, "Could not find wan manager object");
    when_null_trace(wan_obj, exit, ERROR, "Could not find wan mode object");

    amxc_var_init(&data);
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);

    amxd_object_get_params(wanm_obj, &data, amxd_dm_access_protected);
    sense_current_mode = amxd_object_get_value(bool, get_current_wan_mode(), "EnableSensing", NULL);
    amxc_var_add_key(bool, &data, "sense_current_mode", sense_current_mode);
    modes = amxc_var_add_key(amxc_llist_t, &data, "modes", NULL);
    amxd_object_for_each(instance, it, wan_obj) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        // Only add modes that we want to sense
        if(amxd_object_get_value(bool, instance, "EnableSensing", NULL)) {
            SAH_TRACEZ_INFO(ME, "Adding '%s' to module data", instance->name);
            params = amxc_var_add(amxc_htable_t, modes, NULL);
            amxd_object_get_params(instance, params, amxd_dm_access_protected);
        }
    }

    rv = mod_autosensing_execute_function("autosensing-start", &data);

exit:
    amxc_var_clean(&data);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Call to autosensing module to stop the sensing
 * @return Will return 0 if successful and returns an error code when it fails
 */
int mod_autosensing_stop(void) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxc_var_t data;

    amxc_var_init(&data);
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);

    rv = mod_autosensing_execute_function("autosensing-stop", &data);

    amxc_var_clean(&data);
    SAH_TRACEZ_OUT(ME);
    return rv;
}
