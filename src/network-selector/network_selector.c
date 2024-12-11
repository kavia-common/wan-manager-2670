/**
 * @file network_selector.c
 *
 * @brief This file contains implementation of network_selector functionalities and
 *        event handling
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 AT&T
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * Subject to the terms and conditions of this license, each
 * copyright holder and contributor hereby grants to those receiving
 * rights under this license a perpetual, worldwide, non-exclusive,
 * no-charge, royalty-free, irrevocable (except for failure to
 * satisfy the conditions of this license) patent license to make,
 * have made, use, offer to sell, sell, import, and otherwise
 * transfer this software, where such license applies only to those
 * patent claims, already acquired or hereafter acquired, licensable
 * by such copyright holder or contributor that are necessarily
 * infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright
 * holders and non-copyrightable additions of contributors, in
 * source or binary form) alone; or
 *
 * (b) combination of their Contribution(s) with the work of
 * authorship to which such Contribution(s) was added by such
 * copyright holder or contributor, if, at the time the Contribution
 * is added, such addition causes such combination to be necessarily
 * infringed. The patent license shall not apply to any other
 * combinations which include the Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any
 * copyright holder or contributor is granted under this license,
 * whether expressly, by implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxm/amxm.h>
#include "network-selector/network_selector.h"
#include "dm_wan-manager.h"
#include "dm_wan_mode.h"
#include "wan_manager_utils.h"

#define ME "net-ctrl"
#define INVALID_VALUE -1
#define MAX_LENGTH    16

bool network_selector_can_start = true;

/**
 * @brief           network_selector_start
 *
 * @detail          This function reads the WAN Manager Data Model,
 *                  retrieving the WAN object,the current operation mode is "Automatic".
 *                  It then iterates through all WAN instances, invokes the
 *                  `detect-network` function from the `mod-network-selector`.
 *
 * @retval status indicating success or failure
 */
int network_selector_start(void) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    const char* current_operation_mode = NULL;
    amxc_var_t* params = NULL;
    amxc_var_t* modes = NULL;
    amxd_object_t* wanm_obj = NULL;
    amxd_object_t* wan_obj = NULL;

    amxd_trans_t trans;
    amxd_trans_init(&trans);

    amxc_var_t data;
    amxc_var_t rv;

    amxc_var_init(&data);
    amxc_var_init(&rv);

    // Reading the WAN Manager Data Model and verifying it exists.
    // Get the WAN manager object
    wanm_obj = get_wan_manager_obj();
    when_null_trace(wanm_obj, exit, ERROR, "Could not find wan manager object");
    wan_obj = amxd_object_findf(wanm_obj, "WAN.");
    when_null_trace(wan_obj, exit, ERROR, "Could not find wan mode object");

    current_operation_mode = object_const_string(get_wan_manager_obj(), "OperationMode");
    when_str_empty_trace(current_operation_mode, exit, ERROR, "Could not get current operation mode");
    if((strcmp(current_operation_mode, "Automatic") != 0)) {
        goto exit;
    }

    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    modes = amxc_var_add_key(amxc_llist_t, &data, "modes", NULL);

    // Iterate through each instance of the WAN object
    amxd_object_for_each(instance, it, wan_obj) {
        amxd_object_t* wan_instance = amxc_container_of(it, amxd_object_t, it);
        // Add a hash table to the modes variable for the current instance
        params = amxc_var_add(amxc_htable_t, modes, NULL);
        if(params == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failed to add hash table to modes");
            goto exit;
        }
        amxd_object_get_params(wan_instance, params, amxd_dm_access_protected);
    }
    /* Call to mod-network-selector module to get information of the changed wanmode
     * Shared object to be executed is mod-network-selector and function is
     * "detect_network" in the module "mod-network-selector" with args
     * and return value of the executed function.
     * rv is 0, if Shared object executed successfully else an error code.
     */
    status = amxm_execute_function("mod-network-selector",
                                   "mod-network-ctrl",
                                   "detect-network",
                                   &data,
                                   &rv);

    // Check if the function execution was successful
    if(status != amxd_status_ok) {
        SAH_TRACEZ_ERROR(ME, "Failed to call detect-network from mod-network-selector");
    }

exit:
    amxd_trans_clean(&trans);
    amxc_var_clean(&data);
    amxc_var_clean(&rv);
    SAH_TRACEZ_OUT(ME);
    return status;
}
/**
 * @brief                  update_network_selector
 *
 * @detail                 This function handles network selector updates by processing the event data.
 *                         It toggles flags according to the information provided in the event data.
 *
 * @param[in] event_data - A pointer containing event data.
 *
 * @retval                 void
 */
void _update_network_selector(UNUSED const char* const event_name,
                              UNUSED const amxc_var_t* const event_data,
                              UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);

    if(network_selector_can_start == true) {
        network_selector_start();
    } else {
        network_selector_can_start = true;
    }

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief                    network_selector_set_wan_mode
 *
 * @details                  This function takes the WAN mode alias from the provided arguments
 *                           and sets the corresponding WAN mode. If the alias is invalid,
 *                           an error is logged, and the function exits. The current WAN mode
 *                           is retrieved for comparison, and the new mode is applied using the
 *                           WAN mode setting function.
 *
 * @param[in] function_name  Unused parameter.
 * @param[in] args           Contains the "Alias" string, which represents the new WAN mode.
 * @param[in] ret            Unused parameter.
 *
 * @retval    int            Returns 0 on success, or a negative value on failure.
 */
static int network_selector_set_wan_mode(UNUSED const char* function_name,
                                         amxc_var_t* args,
                                         UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    int rv = INVALID_VALUE;

    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    when_null_trace(args, exit, ERROR, "Failed to get 'args' parameter");

    amxd_object_t* wanmanager_obj = amxd_dm_findf(wan_get_dm(), "WANManager.");
    when_null_trace(wanmanager_obj, exit, ERROR, "Could not find WANManager object");

    const char* next_wan_mode_str = GET_CHAR(args, "Alias");
    const char* physical_type = GET_CHAR(args, "PhysicalType");
    const char* current_wan_mode_str = get_current_wan_mode_str();
    const char* wanmode_status = GET_CHAR(args, "Status");

    when_null_trace(next_wan_mode_str, exit, ERROR, "Invalid mode provided");
    when_null_trace(physical_type, exit, ERROR, "PhysicalType[NULL] not found");

    if((strncmp(next_wan_mode_str, current_wan_mode_str, strlen(next_wan_mode_str)) == 0) &&
       (strncmp(wanmode_status, "Enabled", sizeof("Enabled")) == 0)) {
        SAH_TRACEZ_ERROR(ME, "Wan mode '%s' is already set and enabled, skipping set.", next_wan_mode_str);
        rv = amxd_status_ok;
        goto exit;
    }

    SAH_TRACEZ_INFO(ME, "Setting mode '%s' based on network selection", next_wan_mode_str);
    rv = wan_mode_set(next_wan_mode_str, current_wan_mode_str);
    when_failed_trace(rv, exit, ERROR, "Failed to set wan mode '%s'", next_wan_mode_str);

exit:
    amxd_trans_clean(&trans);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief     register_core_functions
 *
 * @detail    This function retrieves the current shared object and module,
 *            and registers core functions within the module. Specifically,
 *            it registers the "set-wan-mode" function by associating it with
 *            the `network_selector_set_wan_mode` function.
 *
 * @retval    int  Returns 0 on success, or -1 on failure.
 */
static int register_core_functions(void) {
    SAH_TRACEZ_IN(ME);
    int rv = INVALID_VALUE;

    amxm_shared_object_t* so = amxm_get_so("self");
    when_null_trace(so, exit, ERROR, "Failed to load shared object 'self'");
    amxm_module_t* mod = amxm_so_get_module(so, MOD_DM_MNGR);
    when_null_trace(mod, exit, ERROR, "Failed to get module 'MOD_DM_MNGR' from shared object");
    rv = amxm_module_add_function(mod, "set-wan-mode", network_selector_set_wan_mode);
    when_failed_trace(rv, exit, ERROR, "Failed to register function set-wan-mode");

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief      mod_network_selector_init
 *
 * @details    This function initializes to start the mod-network-selector module.
 *
 * @return 0 on successful initialization.
 */
int mod_network_selector_init(void) {
    SAH_TRACEZ_IN(ME);
    register_core_functions();
    SAH_TRACEZ_OUT(ME);
    return 0;
}
