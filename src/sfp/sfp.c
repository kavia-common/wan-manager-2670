/**
 * @file sfp.c
 *
 * @brief This file contains implementation of sfp functionalities in wan-manager
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
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>
#include <amxc/amxc_macros.h>
#include <amxm/amxm.h>

#include <amxb/amxb.h>
#include <amxb/amxb_be_mngr.h>
#include <amxb/amxb_register.h>
#include <amxb/amxb_connect.h>

#include "dm_wan_mode.h"
#include "dm_wan-manager.h"
#include "wan_manager_utils.h"
#include "ctrl/restart.h"
#include "sfp/sfp.h"
#include "autosensing/autosensing.h"

#define ME "wanmgr_sfp"
extern bool network_selector_can_start;

/**
 * @brief update_sfp_type
 *
 * @detail Callback function triggered to update SFPType datamodel param.
 *
 * @param[in] args a pointer to the argument list.
 *
 * @return 0 on success
 */
static int update_sfp_type(UNUSED const char* function_name,
                           amxc_var_t* args,
                           UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    const char* sfpType = NULL;
    const char* sfpIntf = NULL;
    const char* wanm_sfpType = NULL;
    const char* supported_sfp = NULL;
    const char* wanIntf = NULL;
    const char* value = "";
    amxd_object_t* wanm_obj = NULL;
    amxd_object_t* winstance = NULL;
    amxd_object_t* intf_instance = NULL;
    int rc = -1;

    amxc_var_t data;
    amxc_var_t retval;
    amxd_trans_t trans;

    amxc_var_init(&data);
    amxc_var_init(&retval);
    amxd_trans_init(&trans);

    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    when_null_trace(args, exit, ERROR, "The Input argument args is NULL");

    // Retrieve attributes of args
    sfpType = GET_CHAR(args, "SFPCategory");
    sfpIntf = GET_CHAR(args, "ifname");
    when_null_trace(sfpType, exit, ERROR, "SFPCategory returned as NULL");
    when_null_trace(sfpIntf, exit, ERROR, "Interface name returned as NULL");
    // Reading WANManager DataModel and get the WANManager object
    wanm_obj = get_wan_manager_obj();
    when_null_trace(wanm_obj, exit, ERROR, "Could not find wan manager object");

    if(strcmp(sfpType, "SFP_UNKNOWN") != 0) {
        SAH_TRACEZ_ERROR(ME, "Stop autosening when SFP detected, SFP %s", sfpType);
        mod_autosensing_stop();
    }

    // Iterating through each instance of the WAN object
    amxd_object_for_each(instance, it, amxd_object_findf(wanm_obj, "WAN.")) {
        // Reading the current WAN mode object
        winstance = amxc_container_of(it, amxd_object_t, it);
        when_null_trace(winstance, exit, ERROR, "Cannot get winstance object");

        // Selecting the WAN mode object for transaction
        amxd_trans_select_object(&trans, winstance);
        wanm_sfpType = object_const_string(winstance, "SFPType");
        supported_sfp = object_const_string(winstance, "SupportedSFPTypes");
        when_null_trace(wanm_sfpType, exit, ERROR, "SFPType returned a NULL value");
        when_null_trace(supported_sfp, exit, ERROR, "SupportedSFPTypes returned a NULL value");

        /*
         * Iterating through each WAN instance interface and updating the sfp type param
         * by comparing SupportedSFPTypes from WANManager.WAN. object and
         * SFPCategory from Device.SFPs.Cage.1.SFP.Transceiver. object
         */
        if(strstr(supported_sfp, sfpType)) {
            // Iterating through each WAN mode interfaces of the WAN object
            amxd_object_for_each(instance, wit, amxd_object_findf(winstance, ".Intf.")) {
                intf_instance = amxc_container_of(wit, amxd_object_t, it);
                when_null_trace(intf_instance, exit, ERROR, "No interface found under WAN instance");

                // Reading the Interface name form the interface object
                wanIntf = amxd_object_get_name(intf_instance, AMXD_OBJECT_NAMED);
                when_str_empty_trace(wanIntf, exit, ERROR, "The interface name is null or empty");
                /* Update SFPType parameter under WANManager.WAN. object
                 * if and only if Interface Name and SupportedSFPTypes matches
                 */
                if(strncmp(wanIntf, sfpIntf, INTF_LENGTH) == 0) {
                    if(strnlen(wanm_sfpType, sizeof(wanm_sfpType)) && strstr(sfpType, wanm_sfpType)) {
                        SAH_TRACEZ_INFO(ME, "SFPType is already updated, returning");
                        rc = amxd_status_ok;
                        goto exit;
                    }
                    value = sfpType;
                }
            }
        }
        /* Updating the SFPType parameter
         * 1. If SFPCategory is SFP_UNKNOWN then SFPType is set as an empty string for all the wan instances.
         * 2. If SFPCategory matches any of the SupportedSFPTypes then SFPType is updated accordingly.
         */
        amxd_trans_set_value(cstring_t, &trans, "SFPType", value);
        // Resetting the value to empty string
        value = "";
    }

    // Do trans_apply to update DM parameter
    rc = amxd_trans_apply(&trans, wan_get_dm());
    if(rc != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to update SFPType parameter in WANManager : '%s'", sfpType);
    } else {
        // network_selector_can_start is set to false when SFP removed or not present
        network_selector_can_start = (strstr(sfpType, "SFP_UNKNOWN") == NULL);
    }

exit:
    amxc_var_clean(&data);
    amxc_var_clean(&retval);
    amxd_trans_clean(&trans);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief wanmgr_sfp_register_core_functions
 *
 * @detail Registers the module namespace and a callback function.
 *
 * @return 0 on success, -1 on failure.
 */
static int wanmgr_sfp_register_core_functions(void) {
    SAH_TRACEZ_IN(ME);
    int rv = amxd_status_unknown_error;
    amxm_shared_object_t* so = amxm_get_so("self");
    when_null_trace(so, exit, ERROR, "Unable to get shared object.");

    // Get the shared object for WANManager
    amxm_module_t* mod = amxm_so_get_module(so, MOD_DM_MNGR);
    when_null_trace(mod, exit, ERROR, "Module" MOD_DM_MNGR "not found");
    rv = amxm_module_add_function(mod, MOD_WANMGR_UPDATE_FUNC, update_sfp_type);
    when_failed_trace(rv, exit, ERROR, "Failed to register function update_sfp_type");
exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief get_sfp_type
 *
 * @detail Executes a function in mod-wanmgr-sfp to get SFP details from tr181-sfpmgr.
 *
 * @return 0 on success, -1 on failure.
 */
int get_sfp_type(void) {
    int rv = amxd_status_unknown_error;
    amxc_var_t data;
    amxc_var_t ret;
    amxc_var_init(&data);
    amxc_var_init(&ret);

    /* Call to mod-wanmgr-sfp module to get the SFP information
     * Shared object to be executed is mod-wanmgr-sfp and function is
     * "get-sfp-type" in the module "mod-wanmgr-sfp" with unused args
     * and return value of the executed function.
     * rv is 0, if Shared object executed successfully else an error code.
     */
    rv = amxm_execute_function("mod-wanmgr-sfp",
                               MOD_WANMGR_SFP,
                               MOD_WANMGR_SFP_GET_FUNC,
                               &data,
                               &ret);
    if(rv != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to execute get-sfp-type function '%d'", rv);
    }

    amxc_var_clean(&data);
    amxc_var_clean(&ret);
    return rv;
}

/**
 * @brief check_sfp_type
 *
 * @detail Executes callback in mod-wanmgr-sfp
 *         to detect SFP and get SFP details from sfpmgr.
 *
 * @return 0 on success
 */
static int check_sfp_type(void) {
    int rv = amxd_status_unknown_error;
    amxc_var_t data;
    amxc_var_init(&data);
    amxc_var_t ret;
    amxc_var_init(&ret);

    /* Call to mod-wanmgr-sfp module to get information of the changed SFP type
     * Shared object to be executed is mod-wanmgr-sfp and function is
     * "sfp-category-change" in the module "mod-wanmgr-sfp" with unused args
     * and return value of the executed function.
     * rv is 0, if Shared object executed successfully else an error code.
     */
    rv = amxm_execute_function("mod-wanmgr-sfp",
                               MOD_WANMGR_SFP,
                               MOD_WANMGR_SFP_CHECK_FUNC,
                               &data,
                               &ret);
    if(rv != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to execute sfp-category-change function '%d'", rv);
    }
    amxc_var_clean(&data);
    amxc_var_clean(&ret);

    return rv;
}

/**
 * @brief read_sfp_category
 *
 * @detail Executes a function in mod-wanmgr-sfp and returns result in ret param.
 *
 * @param[out] ret a pointer to return the SFPCategory.
 *
 * @return 0 on success, -1 on failure.
 */
int read_sfp_category(amxc_var_t* ret) {
    SAH_TRACEZ_IN(ME);
    int rv = amxd_status_unknown_error;
    amxc_var_t data;
    amxc_var_init(&data);

    when_null_trace(ret, exit, ERROR, "argument ret is NULL");

    // Call to mod-wanmgr-sfp module to read SFPCategory.
    rv = amxm_execute_function("mod-wanmgr-sfp",
                               MOD_WANMGR_SFP,
                               MOD_WANMGR_SFP_READ_FUNC,
                               &data,
                               ret);
    when_failed_trace(rv, exit, ERROR, "Failed to execute read-sfpcategory function, '%d'", rv);

exit:
    amxc_var_clean(&data);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief mod_wanmgr_sfp_init
 *
 * @details This function initializes to start the mod-wanmgr-sfp module.
 *
 * @return 0 on successful initialization.
 */
int mod_wanmgr_sfp_init(void) {
    SAH_TRACEZ_IN(ME);
    int rv = amxd_status_unknown_error;

    // Get the registered so for WANManager
    rv = wanmgr_sfp_register_core_functions();
    if(rv != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to execute wanmgr_sfp_register_core_function [%d]", rv);
        return rv;
    }

    // Get information of the changed SFP type
    rv = check_sfp_type();
    if(rv != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to execute check_sfp_type function [%d]", rv);
        return rv;
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}
