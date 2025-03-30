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

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb.h>
#include <amxm/amxm.h>

#include "dm_wan-manager.h"
#include "dm_wan_mode.h"
#include "ctrl/restart.h"
#include "netmodel/nm_query.h"
#include "autosensing/autosensing.h"
#include "wan_manager_utils.h"
#include "component.h"
#include "dns/dns.h"
#include "upstream_intf.h"
#include "ethernet/ethernet.h"
#include "bridge_mode.h"
#include "network-selector/network_selector.h"
#include "sfp/sfp.h"

#define ME "wan-man"

#define NEEDLE_ETHERNET "Device.Ethernet.Interface."
#define NEEDLE_BRIDGE "Device.Bridging.Bridge."
#define NEEDLE_GPON "Device.XPON."
#define NEEDLE_WWAN "Device.Cellular.Interface."

typedef enum {
    WAN_Mode_Enabled = 0,
    WAN_Mode_Disabled,
    WAN_Mode_Error,
    WAN_Mode_Nr_
} wan_mode_status_t;

typedef struct {
    mode_ctrl_t mode;
    const char* str;
} mode_cnv_t;

static mode_cnv_t mode_cnv[] = {
    { IPv4_DHCP, "dhcp4" },
    { IPv4_PPP, "ppp4" },
    { IPv4_STATIC, "static" },
    { IPv4_LINK, "link" },
    { IPv6_DHCP, "dhcp6" },
    { IPv6_PPP, "ppp6" },
    { IPv6_STATIC, "static" },
    { IPv4_DSLITE, "dslite" },
    { IPv6_LINK, "link"},
    { TYPE_VLAN, "vlan" },
    { TYPE_UNTAGGED, "untagged" },
    { TYPE_ATM, "atm" },
    { IP_None, NULL}
};

static const char* wan_mode_status_str[WAN_Mode_Nr_] = {
    "Enabled",
    "Disabled",
    "Error",
};

static amxd_object_t* wan_manager = NULL;
static bool wan_autosensing_can_start = false;

static mode_ctrl_t get_wan_mode_type(amxd_object_t* interface,
                                     bool include_type,
                                     const char* custom_ipv4_mode,
                                     const char* custom_ipv6_mode);
static const char* wan_mode_status_to_str(wan_mode_status_t status);
static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status);
static void update_operation_mode(const char* new_operation_mode, const char* sensing_policy, bool override_boot);
static void startup_wan_autosensing(void);

void update_sensing(void) {
    SAH_TRACEZ_IN(ME);
    const char* current_operation_mode = object_const_string(wan_manager, "OperationMode");
    const char* sensing_policy = object_const_string(wan_manager, "SensingPolicy");
    when_str_empty_trace(current_operation_mode, exit, ERROR, "Could not get current operation mode");
    when_str_empty_trace(sensing_policy, exit, ERROR, "Could not get current sensing policy");

    if((strcmp(current_operation_mode, "Automatic") == 0) && wan_autosensing_can_start) {
        mod_autosensing_stop();
        if(strcmp(sensing_policy, "Continuous") == 0) {
            mod_autosensing_start();
        }
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

void wan_mode_init(void) {
    SAH_TRACEZ_IN(ME);
    amxc_var_t lcontrollers;
    amxc_string_t mod_path;
    amxc_var_init(&lcontrollers);
    amxc_string_init(&mod_path, 0);
    int rv = -1;
    const char* name = NULL;
    amxm_shared_object_t* module_so = NULL;

    wan_manager = amxd_dm_findf(wan_get_dm(), "WANManager");
    when_null_trace(wan_manager, exit, ERROR, "Failed to find the WANManager instance");
    amxd_object_t* wanm_obj = get_wan_manager_obj();
    when_null_trace(wanm_obj, exit, ERROR, "Failed to get the WANManager object");
    amxo_parser_t* parser = wan_get_parser();
    const amxc_var_t* controllers = amxd_object_get_param_value(wanm_obj, "ExtensionModules");
    when_null_trace(controllers, exit, ERROR, "Failed to get the ExtensionModules");
    const char* const mod_dir = GET_CHAR(&parser->config, "external-mod-dir");
    when_str_empty_trace(mod_dir, exit, ERROR, "'external-mod-dir' is NULL or empty");

    amxc_var_convert(&lcontrollers, controllers, AMXC_VAR_ID_LIST);
    amxc_var_for_each(controller, &lcontrollers) {
        name = GET_CHAR(controller, NULL);
        when_null_trace(name, exit, ERROR, "Failed to get controller name, NULL value return");
        amxc_string_setf(&mod_path, "%s/%s.so", mod_dir, name);

        rv = amxm_so_open(&module_so, name, amxc_string_get(&mod_path, 0));
        SAH_TRACEZ_INFO(ME, "Loading controller '%s' %s", name, rv ? "failed" : "successful");
        when_failed(rv, exit);
    }
exit:
    amxc_string_clean(&mod_path);
    amxc_var_clean(&lcontrollers);
    SAH_TRACEZ_OUT(ME);
}

amxd_object_t* get_wan_manager_obj(void) {
    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_OUT(ME);
    return wan_manager;
}

void wan_mode_cleanup(void) {
    SAH_TRACEZ_IN(ME);
    wan_manager = NULL;
    SAH_TRACEZ_OUT(ME);
}

physical_type_t get_physical_type_by_reference(const char* physical_reference) {
    physical_type_t result = physical_type_last;
    if(strstr(physical_reference, NEEDLE_ETHERNET) != NULL) {
        result = physical_type_ethernet;
    } else if(strstr(physical_reference, NEEDLE_BRIDGE) != NULL) {
        result = physical_type_bridge;
    } else if(strstr(physical_reference, NEEDLE_GPON) != NULL) {
        result = physical_type_gpon;
    } else if(strstr(physical_reference, NEEDLE_WWAN) != NULL) {
        result = physical_type_wwan;
    }
    return result;
}

physical_type_t get_physical_type(amxd_object_t* wan_mode) {
    SAH_TRACEZ_IN(ME);
    physical_type_t result = physical_type_last;
    const char* physical_type = NULL;
    const char* physical_reference = NULL;

    when_null_trace(wan_mode, exit, ERROR, "Could not find wan-mode");
    physical_type = object_const_string(wan_mode, "PhysicalType");
    physical_reference = object_const_string(wan_mode, "PhysicalReference");

    if(str_empty(physical_reference)) {
        result = string_to_physical_type(physical_type);
    } else {
        result = get_physical_type_by_reference(physical_reference);
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return result;
}

void wan_manager_found_ll(physical_type_t found_phys_type) {
    SAH_TRACEZ_IN(ME);
    const char* operation_mode = object_const_string(wan_manager, "OperationMode");
    physical_type_t current_phys_type = physical_type_last;
    amxd_object_t* wanm_obj = amxd_dm_findf(wan_get_dm(), "WANManager.");
    bool apply = amxd_object_get_value(bool, wanm_obj, "ApplyAtNextBoot", NULL);
    const char* current_wanmodes = get_current_wan_mode_str();
    amxc_string_t current_wan_modes_str;
    amxc_llist_t current_list;
    bool wan_mode_enabled = false;

    const char* sfp_type = NULL;
    int retval = amxd_status_unknown_error;
    int retval_get = amxd_status_unknown_error;
    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);

    amxc_string_init(&current_wan_modes_str, 0);
    amxc_llist_init(&current_list);

    retval = read_sfp_category(&ret);
    if(retval != 0) {
        SAH_TRACEZ_ERROR(ME, "Unable to read sfp_category, '%d'", retval);
        goto exit;
    }

    sfp_type = GET_CHAR(&ret, "SFPCategory");
    when_null_trace(sfp_type, exit, ERROR, "SFPType returned a NULL value");
    if(strcmp("SFP_UNKNOWN", sfp_type) != 0) {
        retval_get = get_sfp_type();
        if(retval_get != 0) {
            SAH_TRACEZ_ERROR(ME, "Failed to execute get-sfp-type function '%d'", retval_get);
        }
        goto exit;
    }

    if(strcmp(operation_mode, "Automatic") == 0) {
        startup_wan_autosensing();
        goto exit;
    }

    when_str_empty_trace(current_wanmodes, exit, ERROR, "Current wan mode objects could not be found");
    when_false(apply, exit);

    amxc_string_set(&current_wan_modes_str, current_wanmodes);
    amxc_string_split_to_llist(&current_wan_modes_str, &current_list, ',');
    amxc_llist_for_each(it, &current_list) {
        const char* wan_mode = amxc_string_get(amxc_string_from_llist_it(it), 0);
        amxd_object_t* wan_mode_obj = get_wan_mode(wan_mode);
        when_null_trace(wan_mode_obj, exit, ERROR, "Cannot get WANMode object");
        current_phys_type = get_physical_type(wan_mode_obj);
        when_true_trace(current_phys_type == physical_type_last, exit, ERROR, "Failed to get the physical type for the current wan mode");
        if(found_phys_type == current_phys_type) {
            amxd_status_t rc = wan_mode_enable(wan_mode_obj, true);
            if(rc != amxd_status_ok) {
                wan_mode_set_status(wan_mode_obj, WAN_Mode_Error);
                SAH_TRACEZ_WARNING(ME, "Failed to enable current WANMode '%s', error '%d'", wan_mode_obj->name, rc);
            }
            wan_mode_enabled = true;
        }
    }

    if(wan_mode_enabled) {
        amxd_status_t rv = amxd_status_unknown_error;
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, wanm_obj);
        amxd_trans_set_value(bool, &trans, "ApplyAtNextBoot", false);
        rv = amxd_trans_apply(&trans, wan_get_dm());

        amxd_trans_clean(&trans);
        when_failed_trace(rv, exit, ERROR, "Failed to disable 'ApplyAtNextBoot'");
    }

exit:
    amxc_var_clean(&ret);
    amxc_llist_clean(&current_list, amxc_string_list_it_free);
    amxc_string_clean(&current_wan_modes_str);
    SAH_TRACEZ_OUT(ME);
}

static void startup_wan_autosensing(void) {
    SAH_TRACEZ_IN(ME);
    const char* current_operation_mode = NULL;
    const char* sensing_policy = NULL;
    amxd_object_t* wan_obj = amxd_object_get(wan_manager, "WAN");

    when_true(wan_autosensing_can_start, exit);
    when_null_trace(wan_manager, exit, ERROR, "Did not get the wan-manager object yet");
    current_operation_mode = object_const_string(wan_manager, "OperationMode");
    sensing_policy = object_const_string(wan_manager, "SensingPolicy");

    // All WANModes that need to be sensed should have a lower layer
    amxd_object_for_each(instance, it, wan_obj) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        when_null(instance, exit);
        if(amxd_object_get_value(bool, instance, "EnableSensing", NULL)) {
            nm_query_ll_info_t* info = (nm_query_ll_info_t*) instance->priv;
            when_null(info, exit);
            when_str_empty_trace(info->lower_layer, exit, INFO, "Waiting on WANMode %s", amxd_object_get_name(instance, AMXD_OBJECT_NAMED));
        }
    }

    wan_autosensing_can_start = true;
    update_operation_mode(current_operation_mode, sensing_policy, true);

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

static void update_operation_mode(const char* new_operation_mode, const char* sensing_policy, bool override_boot) {
    SAH_TRACEZ_IN(ME);
    when_str_empty_trace(new_operation_mode, exit, ERROR, "Bad new operation mode value");

    SAH_TRACEZ_INFO(ME, "WANManager set sensing mode to %s", new_operation_mode);
    if(0 == strcmp(new_operation_mode, "Automatic")) {
        when_str_empty_trace(sensing_policy, exit, ERROR, "Bad sensing policy value");
        when_false_trace(wan_autosensing_can_start, exit, WARNING, "Not able to start autosensing, physical interface not known yet");
        when_true_trace(!override_boot && strcmp(sensing_policy, "AtBoot") == 0, exit, INFO, "Not starting autosensing since Policy is AtBoot");
        when_true_trace(!override_boot && strcmp(sensing_policy, "Sticky") == 0, exit, INFO, "Not starting autosensing explicitly since Policy is Sticky, waiting on NetModel events");
        mod_autosensing_start();
    } else if(0 == strcmp(new_operation_mode, "Manual")) {
        mod_autosensing_stop();
    } else {
        SAH_TRACEZ_ERROR(ME, "Unsupported operation mode[%s]", new_operation_mode);
    }
exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

/**
 * @brief Allows to the the WANMode and/or the OperationMode in the datamodel
 * @param wan_mode The WANMode that needs to be set, when NULL the current mode will be kept
 * @param operation_mode The OperationMode that needs to be set, when NULL the current mode will be kept
 * @return amxd_status_ok when the all actions are applied, otherwise an other
   error code and no changes in the data model are done.
 */
amxd_status_t wan_mode_dm_set(const char* wan_mode, const char* operation_mode) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_trans_t trans;

    amxd_trans_init(&trans);
    when_null_trace(wan_manager, exit, ERROR, "object should not be NULL, can not set wan mode in datamodel");

    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_select_object(&trans, wan_manager);
    if(wan_mode != NULL) {
        rc = is_valid_mode_list(wan_mode);
        when_failed_trace(rc, exit, ERROR, "Invalid wan mode '%s', mode not set in datamodel", wan_mode);
        amxd_trans_set_value(cstring_t, &trans, "WANMode", wan_mode);
    }
    if(operation_mode != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "OperationMode", operation_mode);
    }
    rc = amxd_trans_apply(&trans, wan_get_dm());

exit:
    amxd_trans_clean(&trans);
    SAH_TRACEZ_OUT(ME);
    return rc;
}


static bool wan_mode_different_physical_type(const char* current_wan_modes, UNUSED const char* new_wan_modes) {
    SAH_TRACEZ_IN(ME);
    bool res = false;
    amxc_string_t active_wan_modes_str;
    amxc_string_t new_wan_modes_str;
    amxc_llist_t new_list;
    amxc_llist_t active_list;
    int new_wan_mode_physical_types[physical_type_last] = {0, 0, 0, 0, 0, 0, 0, 0};
    int active_wan_mode_physical_types[physical_type_last] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i = 0;

    amxc_string_init(&active_wan_modes_str, 0);
    amxc_string_init(&new_wan_modes_str, 0);
    amxc_llist_init(&new_list);
    amxc_llist_init(&active_list);

    when_str_empty(current_wan_modes, exit);
    SAH_TRACEZ_INFO(ME, "Change mode: [From = %s, To = %s]", current_wan_modes, new_wan_modes);

    amxc_string_set(&new_wan_modes_str, new_wan_modes);
    amxc_string_set(&active_wan_modes_str, current_wan_modes);

    amxc_string_split_to_llist(&new_wan_modes_str, &new_list, ',');
    amxc_string_split_to_llist(&active_wan_modes_str, &active_list, ',');
    amxc_llist_for_each(it_active, &active_list) {
        const char* active_mode = amxc_string_get(amxc_string_from_llist_it(it_active), 0);
        amxd_object_t* wan_mode = NULL;
        physical_type_t physical_type = physical_type_last;
        if(str_empty(active_mode)) {
            continue;
        }
        wan_mode = get_wan_mode(active_mode);
        physical_type = get_physical_type(wan_mode);
        if(physical_type == physical_type_last) {
            continue;
        }
        active_wan_mode_physical_types[physical_type]++;
    }
    amxc_llist_for_each(it_new, &new_list) {
        const char* new_mode = amxc_string_get(amxc_string_from_llist_it(it_new), 0);
        amxd_object_t* wan_mode = NULL;
        physical_type_t physical_type = physical_type_last;
        if(str_empty(new_mode)) {
            continue;
        }
        wan_mode = get_wan_mode(new_mode);
        physical_type = get_physical_type(wan_mode);
        if(physical_type == physical_type_last) {
            continue;
        }
        new_wan_mode_physical_types[physical_type]++;
    }

    for(i = 0; i < physical_type_last; i++) {
        // A new physical type is found
        if((new_wan_mode_physical_types[i] > 0) && (active_wan_mode_physical_types[i] == 0)) {
            res = true;
        }
    }

exit:
    amxc_llist_clean(&new_list, amxc_string_list_it_free);
    amxc_llist_clean(&active_list, amxc_string_list_it_free);
    amxc_string_clean(&new_wan_modes_str);
    amxc_string_clean(&active_wan_modes_str);
    SAH_TRACEZ_OUT(ME);
    return res;
}

amxd_status_t wan_mode_set(const char* wan_modes_to_set, const char* active_wan_modes) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t active_wan_modes_str;
    amxc_string_t new_wan_modes_str;
    amxc_llist_t new_list;
    amxc_llist_t active_list;
    bool restart_needed = false;

    amxc_string_init(&active_wan_modes_str, 0);
    amxc_string_init(&new_wan_modes_str, 0);
    amxc_llist_init(&new_list);
    amxc_llist_init(&active_list);

    when_str_empty_status(active_wan_modes, exit, rc = amxd_status_ok);
    when_str_empty_status(wan_modes_to_set, exit, rc = amxd_status_ok);
    SAH_TRACEZ_INFO(ME, "Change mode: [From = %s, To = %s]", active_wan_modes, wan_modes_to_set);


    if(wan_mode_different_physical_type(active_wan_modes, wan_modes_to_set)) {
        restart_needed = true;
    }

    amxc_string_set(&active_wan_modes_str, active_wan_modes);
    amxc_string_set(&new_wan_modes_str, wan_modes_to_set);

    amxc_string_split_to_llist(&new_wan_modes_str, &new_list, ',');

    /*
       First we have to check which of the already active wanmodes need to stay active.
       These ones are removed from active_wan_modes_str, and new_list.
       The end result is that active_wan_mode_str contains a list of wanmodes that are no longer needed
       and new_list contains a list of wanmodes that need to be setup.

       NOTE: When wan_modes_to_set and active_wan_modes are equal,
       all the wanmodes need to be teared down and setup again. This means we have to skip this step.
     */
    if(strcmp(wan_modes_to_set, active_wan_modes) != 0) {
        amxc_llist_for_each(it, &new_list) {
            const char* new_wan_mode = amxc_string_get(amxc_string_from_llist_it(it), 0);
            if(amxc_string_replace(&active_wan_modes_str, new_wan_mode, "", UINT32_MAX) > 0) {
                amxc_llist_it_take(it);
                amxc_string_list_it_free(it);
            }
        }
    }

    /* Disable the WANModes that are no longer needed */
    amxc_string_split_to_llist(&active_wan_modes_str, &active_list, ',');
    amxc_llist_for_each(active_it, &active_list) {
        amxd_object_t* active_wan_mode_obj = NULL;
        const char* new_wan_mode = amxc_string_get(amxc_string_from_llist_it(active_it), 0);
        if(str_empty(new_wan_mode)) {
            continue;
        }
        active_wan_mode_obj = get_wan_mode(new_wan_mode);
        when_null_trace(active_wan_mode_obj, exit, ERROR, "Current wanmode object could not be found");

        rc = wan_mode_enable(active_wan_mode_obj, false);
        when_failed_trace(rc, exit, ERROR, "Failed to disable %s", new_wan_mode);
    }

    rc = wan_mode_dm_set(wan_modes_to_set, NULL);
    when_failed_trace(rc, exit, ERROR, "Failed to set new WANModes '%s' in the datamodel", wan_modes_to_set);

    /* Enable the WANModes that are needed */
    amxc_llist_for_each(new_it, &new_list) {
        amxd_object_t* new_wan_mode_obj = NULL;
        const char* new_wan_mode = amxc_string_get(amxc_string_from_llist_it(new_it), 0);

        new_wan_mode_obj = get_wan_mode(new_wan_mode);
        when_null_trace(new_wan_mode_obj, exit, ERROR, "%s is not a valid WAN mode", new_wan_mode);

        rc = wan_mode_enable(new_wan_mode_obj, true);
        if(rc != amxd_status_ok) {
            SAH_TRACEZ_WARNING(ME, "Failed to enable '%s' as WANMode", wan_modes_to_set);
            wan_mode_set_status(new_wan_mode_obj, WAN_Mode_Error);
            goto exit;
        }
    }

    if(restart_needed) {
        rc = restart();
    }

exit:
    amxc_llist_clean(&active_list, amxc_string_list_it_free);
    amxc_llist_clean(&new_list, amxc_string_list_it_free);
    amxc_string_clean(&new_wan_modes_str);
    amxc_string_clean(&active_wan_modes_str);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief
 * Function that enables or disables the interface of the wan-mode.
 *
 * @param interface Interface to be enabled/disabled
 * @param ll_info struct containing lowerlayer info
 * @param enable Enable parameter
 * @param old_interface Old interface to disable, can be the same as the interface when using ipv4_mode_ovr/ipv6_mode_ovr
 * @param ipv4_mode_ovr IPv4 mode that overrides the current one of the interface. No override done when NULL
 * @param ipv6_mode_ovr IPv6 mode that overrides the current one of the interface. No override done when NULL
 * @return amxd_status_t
 */
static amxd_status_t wan_mode_intf_enable(amxd_object_t* interface,
                                          const char* wanmode_name,
                                          nm_query_ll_info_t* ll_info,
                                          bool enable,
                                          const char* ipv4_mode_ovr,
                                          const char* ipv6_mode_ovr) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t parameters;
    amxc_var_t* ipv4_var = NULL;
    amxc_var_t* ipv6_var = NULL;
    amxd_object_t* ipv4_addr = NULL;
    amxd_object_t* ipv6_addr = NULL;
    mode_ctrl_t mode = IP_None;
    amxc_llist_it_t* it = NULL;
    const char* bridge_reference = NULL;
    char* interface_path = NULL;
    bool bridge = false;
    const char* wan_mode_alias = get_current_wan_mode_str();

    amxc_var_init(&parameters);

    when_null_trace(interface, exit, ERROR, "Cannot get Interface object for WANMode");
    SAH_TRACEZ_INFO(ME, "interface %d (%s)", interface->index, interface->name);

    when_failed(amxd_object_get_params(interface, &parameters, amxd_dm_access_private), exit);

    interface_path = amxd_object_get_path(interface, AMXD_OBJECT_INDEXED | AMXD_OBJECT_TERMINATE);
    amxc_var_add_key(cstring_t, &parameters, "intf_obj_path", interface_path);
    amxc_var_add_key(cstring_t, &parameters, "wanmode_name", wanmode_name);

    it = amxd_object_first_instance(amxd_object_findf(interface, ".IPv4Address."));
    ipv4_addr = amxc_container_of(it, amxd_object_t, it);

    it = amxd_object_first_instance(amxd_object_findf(interface, ".IPv6Address."));
    ipv6_addr = amxc_container_of(it, amxd_object_t, it);

    ipv4_var = amxc_var_add_key(amxc_htable_t, &parameters, "ipv4", NULL);
    ipv6_var = amxc_var_add_key(amxc_htable_t, &parameters, "ipv6", NULL);

    amxd_object_get_params(ipv4_addr, ipv4_var, amxd_dm_access_private);
    amxd_object_get_params(ipv6_addr, ipv6_var, amxd_dm_access_private);

    // Get mode for IPv4 & IPv6
    mode = get_wan_mode_type(interface, true, ipv4_mode_ovr, ipv6_mode_ovr);

    bridge_reference = GET_CHAR(&parameters, "BridgeReference");
    bridge = !str_empty(bridge_reference);
    if(enable && bridge) {
        amxc_var_t* var_intf_path = netmodel_getFirstParameter(bridge_reference, "InterfacePath", NULL, netmodel_traverse_one_level_up);
        when_true_trace(amxc_var_is_null(var_intf_path), exit, ERROR, "Failed to find link layer path for '%s'", bridge_reference);
        amxc_var_add_key(cstring_t, &parameters, "LowerLayer", GET_CHAR(var_intf_path, NULL));
        rc = manage_bridge(bridge_reference, ll_info->upstream_intf_path, enable, mode, GET_UINT32(&parameters, "VlanID"), GET_UINT32(&parameters, "VlanPriority"));

        amxc_var_delete(&var_intf_path);
        when_failed_trace(rc, exit, ERROR, "Failed to add port to bridge, return '%d'", rc);
    } else if(enable && ((mode & TYPE_VLAN) != 0)) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(&parameters, ll_info->lower_layer, GET_UINT32(&parameters, "VlanID"), GET_INT32(&parameters, "VlanPriority"), true, wan_mode_alias);
        amxc_var_add_key(cstring_t, &parameters, "LowerLayer", GET_CHAR(&parameters, "VLANTermination"));
    } else {
        amxc_var_add_key(cstring_t, &parameters, "LowerLayer", ll_info->lower_layer);
    }

    if((mode & (MASK_IPv4 | MASK_IPv6)) != IP_None) {
        rc = mode_ctrl_action(mode, &parameters, enable);
    } else if(bridge) {
        rc = bridge_mode_ctrl_action(&parameters, enable);
    }

    if(!enable && bridge) {
        rc = manage_bridge(bridge_reference, ll_info->upstream_intf_path, enable, mode, GET_UINT32(&parameters, "VlanID"), GET_UINT32(&parameters, "VlanPriority"));
        when_failed_trace(rc, exit, ERROR, "Failed to disable port from bridge, return '%d'", rc);
    } else if(!enable && ((mode & TYPE_VLAN) != 0)) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(&parameters, ll_info->lower_layer, GET_UINT32(&parameters, "VlanID"), GET_INT32(&parameters, "VlanPriority"), false, wan_mode_alias);
    }

exit:
    free(interface_path);
    amxc_var_clean(&parameters);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief Enables or disables a wan mode
 * @param wan_mode The datamodel object for the mode that should be enabled/disabled
 * @param enable Enables the mode if set to true, otherwise it disables the mode
 * @return amxd_status_ok if the mode was enabled/disabled correctly, otherwise it returns an error
 */
amxd_status_t wan_mode_enable(amxd_object_t* wan_mode, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    nm_query_ll_info_t* info = NULL;

    when_null_trace(wan_mode, exit, ERROR, "bad wan mode object given");

    info = (nm_query_ll_info_t*) wan_mode->priv;
    when_null_trace(info, exit, ERROR, "No info structure found for physical type '%s'", physical_type_to_string(get_physical_type(wan_mode)));
    when_str_empty_trace(info->lower_layer, exit, ERROR, "LowerLayer for PhysicalType %s returned empty (or null)", physical_type_to_string(get_physical_type(wan_mode)));

    if(!enable) {
        wan_mode_set_status(wan_mode, WAN_Mode_Disabled);
    } else {
        toggle_upstream_intf(info, true);
    }

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        const char* wanmode_name = amxd_object_get_name(wan_mode, AMXD_OBJECT_NAMED);
        rc = wan_mode_intf_enable(interface, wanmode_name, info, enable, NULL, NULL);
        when_failed_trace(rc, exit, ERROR, "Failed to %s interface '%s' with code %d", enable ? "enable" : "disable",
                          amxd_object_get_name(interface, AMXD_OBJECT_NAMED), rc);
    }

    if(enable) {
        rc = dns_mode_set(wan_mode);
        nm_query_mode_active();
    } else {
        rc = dns_mode_unset();
        nm_close_sensing_queries();
    }
    when_failed_trace(rc, exit, ERROR, "failed with code %d, unable to %s the DNS mode", rc, enable ? "set" : "unset");

exit:
    if(enable) {
        wan_mode_set_status(wan_mode, (amxd_status_ok == rc ? WAN_Mode_Enabled : WAN_Mode_Error));
    }
    SAH_TRACEZ_OUT(ME);
    return rc;
}

/**
 * @brief This function can be used to get datamodel object for a specific mode
 * @param alias The alias of the requested mode
 * @return A pointer to the amxd_object_t for the requested WANMode, NULL if no mode was found with this alias
 */
amxd_object_t* get_wan_mode(const char* alias) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* wan_mode_obj = NULL;
    when_str_empty_trace(alias, exit, ERROR, "Could not find wan mode, no Alias provided");
    wan_mode_obj = amxd_dm_findf(wan_get_dm(), "WANManager.WAN.[Alias=='%s'].", alias);

exit:
    SAH_TRACEZ_OUT(ME);
    return wan_mode_obj;
}

/**
 * @brief This function can be used to get the string value contained in the WANMode object
 * @return A string is returned containing the current WANMode
 */
const char* get_current_wan_mode_str(void) {
    SAH_TRACEZ_IN(ME);
    const char* current_wan_mode_str = NULL;

    when_null(wan_manager, exit);
    current_wan_mode_str = object_const_string(wan_manager, "WANMode");

exit:
    SAH_TRACEZ_OUT(ME);
    return current_wan_mode_str;
}

static const char* wan_mode_status_to_str(wan_mode_status_t status) {
    SAH_TRACEZ_IN(ME);
    const char* rv = ((WAN_Mode_Enabled <= status) && (WAN_Mode_Nr_ > status))
        ? wan_mode_status_str[status] : wan_mode_status_str[WAN_Mode_Error];
    SAH_TRACEZ_OUT(ME);
    return rv;
}

static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_trans_t trans;
    const char* str_status = wan_mode_status_to_str(status);
    amxd_trans_init(&trans);

    when_null_trace(object, exit, ERROR, "No object provided, status not set");

    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_select_object(&trans, object);
    amxd_trans_set_value(cstring_t, &trans, "Status", str_status);
    rc = amxd_trans_apply(&trans, wan_get_dm());
    when_failed_trace(rc, exit, ERROR, "Failed to set status '%d(%s)' on object '%s'", status, str_status, object->name);

exit:
    amxd_trans_clean(&trans);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

mode_ctrl_t wan_mode_convert_from_str(const char* mode, const ipversion_t ipversion) {
    SAH_TRACEZ_IN(ME);
    mode_cnv_t* lookup = mode_cnv;
    mode_ctrl_t rc = IP_None;

    when_false(ipversion_valid(ipversion), exit);
    when_null(mode, exit);

    while(lookup->str != NULL) {
        if(0 == strcmp(mode, lookup->str)) {
            rc = lookup->mode;
            // keywords static and link are used in both parameter IPv4Mode & IPv6Mode
            if((rc == IPv4_STATIC) && (ipversion != IPv4)) {
                rc = IPv6_STATIC;
            } else if((rc == IPv4_LINK) && (ipversion != IPv4)) {
                rc = IPv6_LINK;
            }
            break;
        }
        lookup++;
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static mode_ctrl_t get_wan_mode_type(amxd_object_t* interface,
                                     bool include_type,
                                     const char* custom_ipv4_mode,
                                     const char* custom_ipv6_mode) {
    SAH_TRACEZ_IN(ME);
    const char* type = NULL;
    const char* ipv4mode = NULL;
    const char* ipv6mode = NULL;
    int mode = (int) IP_None;
    amxc_var_t data;

    amxc_var_init(&data);
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    amxd_object_get_params(interface, &data, amxd_dm_access_protected);

    if(include_type == true) {
        type = GET_CHAR(&data, "Type");
        when_str_empty_trace(type, exit, ERROR, "Empty 'Type' parameter");
        mode = (int) wan_mode_convert_from_str(type, IPv6);
    }

    ipv4mode = custom_ipv4_mode == NULL ? GET_CHAR(&data, "IPv4Mode") : custom_ipv4_mode;
    when_str_empty_trace(ipv4mode, exit, ERROR, "Empty 'IPv4Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv4mode, IPv4);

    ipv6mode = custom_ipv6_mode == NULL ? GET_CHAR(&data, "IPv6Mode") : custom_ipv6_mode;
    when_str_empty_trace(ipv6mode, exit, ERROR, "Empty 'IPv6Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv6mode, IPv6);

    SAH_TRACEZ_INFO(ME, "Type '%s', ipv4 '%s', ipv6 '%s': mode %#06X",
                    include_type ? type : "", ipv4mode, ipv6mode, mode);

exit:
    amxc_var_clean(&data);
    SAH_TRACEZ_OUT(ME);
    return (mode_ctrl_t) mode;
}

void _update_autosensing(UNUSED const char* const event_name,
                         const amxc_var_t* const event_data,
                         UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    const char* sensing_policy = object_const_string(wan_manager, "SensingPolicy");

    SAH_TRACEZ_INFO(ME, "Toggling OperationMode from %s to %s",
                    GETP_CHAR(event_data, "parameters.OperationMode.from"),
                    GETP_CHAR(event_data, "parameters.OperationMode.to"));

    update_operation_mode(GETP_CHAR(event_data, "parameters.OperationMode.to"), sensing_policy, false);
    SAH_TRACEZ_OUT(ME);
}

void _update_sensing_policy(UNUSED const char* const event_name,
                            UNUSED const amxc_var_t* const event_data,
                            UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    update_sensing();
    SAH_TRACEZ_OUT(ME);
}

void _wan_sensing_toggled(UNUSED const char* const event_name,
                          UNUSED const amxc_var_t* const event_data,
                          UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    update_sensing();
    SAH_TRACEZ_OUT(ME);
}

void _wan_sensing_priority_changed(UNUSED const char* const event_name,
                                   UNUSED const amxc_var_t* const event_data,
                                   UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    update_sensing();
    SAH_TRACEZ_OUT(ME);
}

static void ip_mode_toggled(const amxc_var_t* const event_data, const ipversion_t ipversion) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    nm_query_ll_info_t* info = NULL;
    amxd_object_t* wan_mode_obj = NULL;
    amxd_object_t* intf_obj = NULL;
    amxc_var_t* ip_mode_arg = NULL;
    const char* new_ip_mode = NULL;
    const char* ipv4_mode_ovr = NULL;
    const char* ipv6_mode_ovr = NULL;
    const char* current_operation_mode = object_const_string(get_wan_manager_obj(), "OperationMode");
    const char* wan_mode_name = NULL;
    const char* current_wan_mode = get_current_wan_mode_str();

    when_false(ipversion_valid(ipversion), exit);
    when_null_trace(current_wan_mode, exit, ERROR, "Could not get current wan modes");
    when_str_empty_trace(current_operation_mode, exit, ERROR, "Could not get current operation mode");
    when_true_trace(strcmp(current_operation_mode, "Automatic") == 0, exit, WARNING, "Ignoring changes made when autosensing is active");

    intf_obj = amxd_dm_signal_get_object(wan_get_dm(), event_data);
    when_null_trace(intf_obj, exit, ERROR, "Could not get the interface object");
    wan_mode_obj = amxd_object_get_parent(amxd_object_get_parent(intf_obj));
    when_null_trace(wan_mode_obj, exit, ERROR, "Could not get the wanmode object");

    wan_mode_name = amxd_object_get_name(wan_mode_obj, AMXD_OBJECT_NAMED);
    when_str_empty_trace(wan_mode_name, exit, ERROR, "The event WANMode is null or empty");

    ip_mode_arg = GET_ARG(GET_ARG(event_data, "parameters"), ipversion == IPv4 ? "IPv4Mode" : "IPv6Mode");
    new_ip_mode = GET_CHAR(ip_mode_arg, "to");
    if(ipversion == IPv4) {
        ipv4_mode_ovr = GET_CHAR(ip_mode_arg, "from");
    } else {
        ipv6_mode_ovr = GET_CHAR(ip_mode_arg, "from");
    }

    // If the event WANMode is different than the current WANMode, then do nothing
    if(strcmp(wan_mode_name, current_wan_mode) != 0) {
        SAH_TRACEZ_WARNING(ME, "The event WANMode {%s} is different than current: %s ", wan_mode_name, current_wan_mode);
        rc = amxd_status_ok;
        goto exit;
    }

    info = (nm_query_ll_info_t*) wan_mode_obj->priv;
    when_null_trace(info, exit, ERROR, "No info structure found");

    // Disable the previous ip mode of the interface
    rc = wan_mode_intf_enable(intf_obj, wan_mode_name, info, false, ipv4_mode_ovr, ipv6_mode_ovr);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPMode '%s' in the datamodel", GET_CHAR(ip_mode_arg, "from"));
    // Enable the current ip mode of the interface object
    rc = wan_mode_intf_enable(intf_obj, wan_mode_name, info, true, NULL, NULL);
    when_failed_trace(rc, exit, ERROR, "Failed to enable '%s' as IPMode", new_ip_mode);

    (void) new_ip_mode;

exit:
    if(rc != amxd_status_ok) {
        wan_mode_set_status(wan_mode_obj, WAN_Mode_Error);
    }
    SAH_TRACEZ_OUT(ME);
    return;
}

void _ipv4_mode_toggled(UNUSED const char* const event_name,
                        const amxc_var_t* const event_data,
                        UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    ip_mode_toggled(event_data, IPv4);
    SAH_TRACEZ_OUT(ME);
    return;
}

void _ipv6_mode_toggled(UNUSED const char* const event_name,
                        const amxc_var_t* const event_data,
                        UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    ip_mode_toggled(event_data, IPv6);
    SAH_TRACEZ_OUT(ME);
    return;
}

void _app_start(UNUSED const char* const event_name,
                UNUSED const amxc_var_t* const event_data,
                UNUSED void* const priv) {
    amxd_object_t* wanm_obj = get_wan_manager_obj();
    amxd_object_t* wan_obj = amxd_object_get(wanm_obj, "WAN");

    SAH_TRACEZ_INFO(ME, "app:start event is triggered");

    amxd_object_for_each(instance, it, wan_obj) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        nm_query_ll_add(instance);
    }
}
