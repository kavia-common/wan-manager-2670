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

#define ME "wan-man"
typedef enum {
    WAN_Mode_Enabled = 0,
    WAN_Mode_Disabled,
    WAN_Mode_Error,
    WAN_Mode_Nr_
} wan_mode_status_t;

typedef enum {
    OPERATION_MODE_UNKNOWN,
    OPERATION_MODE_AUTOMATIC,
    OPERATION_MODE_MANUAL
} operation_mode_t;

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

static amxd_object_t* wan_manager;
static bool wan_autosensing_can_start = false;

static mode_ctrl_t get_wan_mode_type(amxd_object_t* interface,
                                     bool include_type,
                                     const char* custom_ipv4_mode,
                                     const char* custom_ipv6_mode);
static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode);
static mode_ctrl_t wan_mode_convert_from_str(const char* mode, bool ipv4);
static const char* wan_mode_status_to_str(wan_mode_status_t status);
static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status);
static operation_mode_t update_operation_mode(const char* new_operation_mode);
static operation_mode_t startup_wan_autosensing(void);

static void update_sensing(void) {
    SAH_TRACEZ_IN(ME);
    char* current_operation_mode = amxd_object_get_value(cstring_t, wan_manager, "OperationMode", NULL);
    char* sensing_policy = amxd_object_get_value(cstring_t, wan_manager, "SensingPolicy", NULL);
    when_str_empty_trace(current_operation_mode, exit, ERROR, "Could not get current operation mode");
    when_str_empty_trace(sensing_policy, exit, ERROR, "Could not get current sensing policy");

    if((strcmp(current_operation_mode, "Automatic") == 0) && wan_autosensing_can_start) {
        mod_autosensing_stop();
        if(strcmp(sensing_policy, "Continuous") == 0) {
            mod_autosensing_start();
        }
    }

exit:
    free(current_operation_mode);
    free(sensing_policy);
    SAH_TRACEZ_OUT(ME);
    return;
}

static amxd_status_t enable_current_wan_mode(void) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* current_wan_mode_obj = get_current_wan_mode();

    when_null_trace(current_wan_mode_obj, exit, ERROR, "Current wan mode object could not be found");

    rc = wan_mode_enable(current_wan_mode_obj, NULL, true);
    if(rc != amxd_status_ok) {
        wan_mode_set_status(current_wan_mode_obj, WAN_Mode_Error);
        SAH_TRACEZ_WARNING(ME, "Failed to enable current WANMode '%s', error '%d'", current_wan_mode_obj->name, rc);
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

void wan_mode_init(void) {
    SAH_TRACEZ_IN(ME);

    wan_manager = amxd_dm_findf(wan_get_dm(), "WANManager");
    when_null_trace(wan_manager, exit, ERROR, "Failed to find the WANManager instance");
    nm_query_ll_init();
exit:
    SAH_TRACEZ_OUT(ME);
    return;
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

static char* get_physical_type_for_current_wan_mode(void) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* wan_mode_inst = get_current_wan_mode();
    char* physical_type = NULL;

    when_null_trace(wan_mode_inst, exit, ERROR, "Could not find the current wan-mode");
    physical_type = amxd_object_get_value(cstring_t, wan_mode_inst, "PhysicalType", NULL);
exit:
    SAH_TRACEZ_OUT(ME);
    return physical_type;
}

void wan_manager_found_ll(const char* found_phys_type) {
    SAH_TRACEZ_IN(ME);
    char* current_phys_type = NULL;
    operation_mode_t operation_mode = startup_wan_autosensing();

    current_phys_type = get_physical_type_for_current_wan_mode();
    when_null_trace(current_phys_type, exit, ERROR, "Failed to get the physical type for the current wan mode");

    if((strcmp(found_phys_type, current_phys_type) == 0) && (operation_mode != OPERATION_MODE_AUTOMATIC)) {
        enable_current_wan_mode();
    }
exit:
    free(current_phys_type);
    SAH_TRACEZ_OUT(ME);
}

static operation_mode_t startup_wan_autosensing(void) {
    SAH_TRACEZ_IN(ME);
    operation_mode_t rv = OPERATION_MODE_UNKNOWN;
    const amxc_var_t* var_operation_mode = NULL;

    when_true(wan_autosensing_can_start, exit);
    when_null_trace(wan_manager, exit, ERROR, "Did not get the wan-manager object yet");

    wan_autosensing_can_start = true;
    var_operation_mode = amxd_object_get_param_value(wan_manager, "OperationMode");
    rv = update_operation_mode(GET_CHAR(var_operation_mode, NULL));

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

static operation_mode_t update_operation_mode(const char* new_operation_mode) {
    SAH_TRACEZ_IN(ME);
    operation_mode_t rv = OPERATION_MODE_UNKNOWN;
    when_str_empty_trace(new_operation_mode, exit, ERROR, "Bad new operation mode value");

    SAH_TRACEZ_INFO(ME, "WANManager set sensing mode to %s", new_operation_mode);
    if(0 == strcmp(new_operation_mode, "Automatic")) {
        when_false_trace(wan_autosensing_can_start, exit, WARNING, "Not able to start autosensing, physical interface not known yet");
        mod_autosensing_start();
        rv = OPERATION_MODE_AUTOMATIC;
    } else if(0 == strcmp(new_operation_mode, "Manual")) {
        mod_autosensing_stop();
        rv = OPERATION_MODE_MANUAL;
    } else {
        SAH_TRACEZ_ERROR(ME, "Unsupported operation mode[%s]", new_operation_mode);
    }
exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
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
        rc = is_valid_mode(wan_mode);
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

amxd_status_t wan_mode_set(const char* wan_mode_to_set, const char* active_wan_mode) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* active_wan_mode_obj = NULL;
    amxd_object_t* new_wan_mode_obj = NULL;

    SAH_TRACEZ_INFO(ME, "Change mode: [From = %s, To = %s]", active_wan_mode, wan_mode_to_set);
    active_wan_mode_obj = get_wan_mode(active_wan_mode);
    when_null_trace(active_wan_mode_obj, exit, ERROR, "Current wanmode object could not be found");
    new_wan_mode_obj = get_wan_mode(wan_mode_to_set);
    when_null_trace(new_wan_mode_obj, exit, ERROR, "%s is not a valid WAN mode", wan_mode_to_set);

    rc = wan_mode_enable(active_wan_mode_obj, NULL, false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the previous wan mode");

    rc = wan_mode_dm_set(wan_mode_to_set, NULL);
    when_failed_trace(rc, exit, ERROR, "Failed to set new WANMode '%s' in the datamodel", wan_mode_to_set);
    rc = wan_mode_enable(new_wan_mode_obj, active_wan_mode_obj, true);
    when_failed_trace(rc, exit, WARNING, "Failed to enable '%s' as WANMode", wan_mode_to_set);

    if(wan_mode_different_physical_type(active_wan_mode_obj, new_wan_mode_obj)) {
        rc = restart();
    }

exit:
    if(rc != amxd_status_ok) {
        wan_mode_set_status(new_wan_mode_obj, WAN_Mode_Error);
    }
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
                                          nm_query_ll_info_t* ll_info,
                                          bool enable,
                                          amxd_object_t* old_interface,
                                          const char* ipv4_mode_ovr,
                                          const char* ipv6_mode_ovr) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t parameters;
    amxc_var_t* old_params = NULL;
    amxc_var_t* ipv4_var = NULL;
    amxc_var_t* ipv6_var = NULL;
    amxd_object_t* ipv4_addr = NULL;
    amxd_object_t* ipv6_addr = NULL;
    mode_ctrl_t mode = IP_None;
    amxc_llist_it_t* it = NULL;
    const char* bridge_reference = NULL;
    bool bridge = false;

    amxc_var_init(&parameters);

    when_null_trace(interface, exit, ERROR, "Cannot get Interface object for WANMode");
    SAH_TRACEZ_INFO(ME, "interface %d (%s)", interface->index, interface->name);

    when_failed(amxd_object_get_params(interface, &parameters, amxd_dm_access_private), exit);

    it = amxd_object_first_instance(amxd_object_findf(interface, ".IPv4Address."));
    ipv4_addr = amxc_container_of(it, amxd_object_t, it);

    it = amxd_object_first_instance(amxd_object_findf(interface, ".IPv6Address."));
    ipv6_addr = amxc_container_of(it, amxd_object_t, it);

    ipv4_var = amxc_var_add_key(amxc_htable_t, &parameters, "ipv4", NULL);
    ipv6_var = amxc_var_add_key(amxc_htable_t, &parameters, "ipv6", NULL);

    amxd_object_get_params(ipv4_addr, ipv4_var, amxd_dm_access_private);
    amxd_object_get_params(ipv6_addr, ipv6_var, amxd_dm_access_private);

    old_params = amxc_var_add_key(amxc_htable_t, &parameters, "old_interface_parameters", NULL);
    amxd_object_get_params(old_interface, old_params, amxd_dm_access_private);

    // Get mode for IPv4 & IPv6
    mode = get_wan_mode_type(interface, true, ipv4_mode_ovr, ipv6_mode_ovr);

    bridge_reference = GET_CHAR(&parameters, "BridgeReference");
    bridge = !STRING_EMPTY(bridge_reference);
    if(enable && bridge) {
        amxc_var_t* var_intf_path = netmodel_getFirstParameter(bridge_reference, "InterfacePath", NULL, netmodel_traverse_one_level_up);
        when_true_trace(amxc_var_is_null(var_intf_path), exit, ERROR, "Failed to find link layer path for '%s'", bridge_reference);
        amxc_var_add_key(cstring_t, &parameters, "LowerLayer", GET_CHAR(var_intf_path, NULL));
        rc = manage_bridge(bridge_reference, ll_info->upstream_intf_path, enable, mode, GET_UINT32(&parameters, "VlanID"), GET_UINT32(&parameters, "VlanPriority"));

        amxc_var_delete(&var_intf_path);
        when_failed_trace(rc, exit, ERROR, "Failed to add port to bridge, return '%d'", rc);
    } else if(enable && ((mode & TYPE_VLAN) != 0)) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(&parameters, ll_info->lower_layer, GET_UINT32(&parameters, "VlanID"), GET_INT32(&parameters, "VlanPriority"), true);
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
        ethernet_vlan_set_enable(&parameters, ll_info->lower_layer, GET_UINT32(&parameters, "VlanID"), GET_INT32(&parameters, "VlanPriority"), false);
    }

exit:
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
amxd_status_t wan_mode_enable(amxd_object_t* wan_mode, amxd_object_t* old_wan_mode, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    nm_query_ll_info_t* info = NULL;
    char* physical_type = NULL;
    char* dns_mode = NULL;

    when_null_trace(wan_mode, exit, ERROR, "bad wan mode object given");
    physical_type = amxd_object_get_value(cstring_t, wan_mode, "PhysicalType", NULL);

    info = get_nm_query_info(physical_type);
    when_null_trace(info, exit, ERROR, "No info structure found for physical type '%s'", physical_type);

    dns_mode = amxd_object_get_value(cstring_t, wan_mode, "DNSMode", NULL);
    when_str_empty_trace(info->lower_layer, exit, ERROR, "LowerLayer for PhysicalType %s returned empty (or null)", physical_type);

    if(!enable) {
        wan_mode_set_status(wan_mode, WAN_Mode_Disabled);
    } else {
        toggle_upstream_intf(physical_type, true);
    }

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        amxd_object_t* old_interface = NULL;
        if(old_wan_mode != NULL) {
            char* intf_name = amxd_object_get_value(cstring_t, interface, "Name", NULL);

            old_interface = amxd_object_findf(old_wan_mode, ".Intf.[Name == '%s'].", intf_name);
            free(intf_name);
        }
        rc = wan_mode_intf_enable(interface, info, enable, old_interface, NULL, NULL);
        when_failed_trace(rc, exit, ERROR, "Failed to %s interface '%s' with code %d", enable ? "enable" : "disable",
                          amxd_object_get_name(interface, AMXD_OBJECT_NAMED), rc);
    }

    if(enable) {
        rc = dns_mode_set(wan_mode, dns_mode);
        nm_query_mode_active();
    } else {
        rc = dns_mode_unset(wan_mode, dns_mode);
        nm_close_sensing_queries();
        toggle_upstream_intf(physical_type, false);
    }
    when_failed_trace(rc, exit, ERROR, "failed with code %d, unable to %s the DNS mode", rc, enable ? "set" : "unset");

exit:
    free(physical_type);
    free(dns_mode);
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
 * @return A string is returned containing the current WANMode, the string must be freed when no longer needed
 */
char* get_current_wan_mode_str(void) {
    SAH_TRACEZ_IN(ME);
    char* current_wan_mode_str = NULL;

    when_null(wan_manager, exit);
    current_wan_mode_str = amxd_object_get_value(cstring_t, wan_manager, "WANMode", NULL);

exit:
    SAH_TRACEZ_OUT(ME);
    return current_wan_mode_str;
}

/**
 * @brief This function can be used to get datamodel object for the mode currently set in the WANMode parameter
 * @return The pointer to the amxd_object_t for the WANMode that is currently configured
 */
amxd_object_t* get_current_wan_mode(void) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* wan_mode_obj = NULL;
    char* current_wan_mode_str = get_current_wan_mode_str();

    when_str_empty_trace(current_wan_mode_str, exit, ERROR, "Failed to get the current wanmode");

    wan_mode_obj = get_wan_mode(current_wan_mode_str);
    when_null_trace(wan_mode_obj, exit, ERROR, "Cannot get current WANMode object");

exit:
    free(current_wan_mode_str);
    SAH_TRACEZ_OUT(ME);
    return wan_mode_obj;
}

static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode) {
    SAH_TRACEZ_IN(ME);
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
    SAH_TRACEZ_OUT(ME);
    return rc;
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

mode_ctrl_t wan_mode_convert_from_str(const char* mode, bool ipv4) {
    SAH_TRACEZ_IN(ME);
    mode_cnv_t* lookup = mode_cnv;
    mode_ctrl_t rc = IP_None;
    while(lookup->str != NULL) {
        if(0 == strcmp(mode, lookup->str)) {
            rc = lookup->mode;
            // keywords static and link are used in both parameter IPv4Mode & IPv6Mode
            if((rc == IPv4_STATIC) && (ipv4 == false)) {
                rc = IPv6_STATIC;
            } else if((rc == IPv4_LINK) && (ipv4 == false)) {
                rc = IPv6_LINK;
            }
            break;
        }
        lookup++;
    }
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
        mode = (int) wan_mode_convert_from_str(type, false);
    }

    ipv4mode = custom_ipv4_mode == NULL ? GET_CHAR(&data, "IPv4Mode") : custom_ipv4_mode;
    when_str_empty_trace(ipv4mode, exit, ERROR, "Empty 'IPv4Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv4mode, true);

    ipv6mode = custom_ipv6_mode == NULL ? GET_CHAR(&data, "IPv6Mode") : custom_ipv6_mode;
    when_str_empty_trace(ipv6mode, exit, ERROR, "Empty 'IPv6Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv6mode, false);

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
    SAH_TRACEZ_INFO(ME, "Toggling OperationMode from %s to %s",
                    GETP_CHAR(event_data, "parameters.OperationMode.from"),
                    GETP_CHAR(event_data, "parameters.OperationMode.to"));
    update_operation_mode(GETP_CHAR(event_data, "parameters.OperationMode.to"));
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

amxd_status_t _mode_check_default_interface(amxd_object_t* object,
                                            amxd_param_t* param,
                                            amxd_action_t reason,
                                            const amxc_var_t* const args,
                                            amxc_var_t* const retval,
                                            void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_unknown_error;
    bool default_interface_new;
    bool default_interface_old;

    if(reason != action_param_validate) {
        rv = amxd_status_invalid_action;
        goto exit;
    }

    // checks if value can be converted to parameter type.
    rv = amxd_action_param_validate(object, param, reason, args, retval, priv);
    when_failed(rv, exit);

    default_interface_new = GET_BOOL(args, NULL);
    default_interface_old = GET_BOOL(&param->value, NULL);

    // Verify that there is no other interface already configured as the default
    if(default_interface_new && !default_interface_old) {
        amxd_object_t* intf_obj = amxd_object_findf(object, "^.[DefaultInterface == true]");
        if(intf_obj != NULL) {
            SAH_TRACEZ_WARNING(ME, "This mode already has a default interface");
            rv = amxd_status_invalid_value;
            goto exit;
        }
        SAH_TRACEZ_INFO(ME, "%s will be used as the default interface", object->name);
        rv = amxd_status_ok;
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

static void ip_mode_toggled(const amxc_var_t* const event_data, bool ipv4) {
    SAH_TRACEZ_IN(ME);
    const char* ip_type = ipv4 ? "IPv4Mode" : "IPv6Mode";
    amxd_status_t rc = amxd_status_unknown_error;
    nm_query_ll_info_t* info = NULL;
    amxd_object_t* wan_mode_obj = NULL;
    amxd_object_t* intf_obj = NULL;
    amxc_var_t* ip_mode_arg = GET_ARG(GET_ARG(event_data, "parameters"), ip_type);
    const char* new_ip_mode = NULL;
    const char* ipv4_mode_ovr = NULL;
    const char* ipv6_mode_ovr = NULL;
    char* wan_status = NULL;
    char* physical_type = NULL;
    char* current_operation_mode = amxd_object_get_value(cstring_t, get_wan_manager_obj(), "OperationMode", NULL);

    new_ip_mode = GET_CHAR(ip_mode_arg, "to");
    if(ipv4) {
        ipv4_mode_ovr = GET_CHAR(ip_mode_arg, "from");
    } else {
        ipv6_mode_ovr = GET_CHAR(ip_mode_arg, "from");
    }

    when_str_empty_trace(current_operation_mode, exit, ERROR, "Could not get current operation mode");
    when_true_trace(strcmp(current_operation_mode, "Automatic") == 0, exit, WARNING, "Ignoring changes made when autosensing is active");

    intf_obj = amxd_dm_signal_get_object(wan_get_dm(), event_data);
    when_null_trace(intf_obj, exit, ERROR, "Could not get the interface object");
    wan_mode_obj = amxd_object_get_parent(amxd_object_get_parent(intf_obj));
    when_null_trace(wan_mode_obj, exit, ERROR, "Could not get the wanmode object");

    wan_status = amxd_object_get_value(cstring_t, wan_mode_obj, "Status", NULL);

    // If the status of the wanmode is disabled, then do nothing
    when_true_status(strcmp(wan_status, "Disabled") == 0, exit, rc = amxd_status_ok);
    physical_type = amxd_object_get_value(cstring_t, wan_mode_obj, "PhysicalType", NULL);

    info = get_nm_query_info(physical_type);
    when_null_trace(info, exit, ERROR, "No info structure found for PhysicalType '%s'", physical_type);

    // Disable the previous ip mode of the interface
    rc = wan_mode_intf_enable(intf_obj, info, false, intf_obj, ipv4_mode_ovr, ipv6_mode_ovr);
    when_failed_trace(rc, exit, ERROR, "Failed to disable IPMode '%s' in the datamodel", GET_CHAR(ip_mode_arg, "from"));
    // Enable the current ip mode of the interface object
    rc = wan_mode_intf_enable(intf_obj, info, true, intf_obj, NULL, NULL);
    when_failed_trace(rc, exit, ERROR, "Failed to enable '%s' as IPMode", new_ip_mode);

    (void) new_ip_mode;

exit:
    if(rc != amxd_status_ok) {
        wan_mode_set_status(wan_mode_obj, WAN_Mode_Error);
    }
    free(current_operation_mode);
    free(physical_type);
    free(wan_status);
    SAH_TRACEZ_OUT(ME);
    return;
}

void _ipv4_mode_toggled(UNUSED const char* const event_name,
                        const amxc_var_t* const event_data,
                        UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    ip_mode_toggled(event_data, true);
    SAH_TRACEZ_OUT(ME);
    return;
}

void _ipv6_mode_toggled(UNUSED const char* const event_name,
                        const amxc_var_t* const event_data,
                        UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    ip_mode_toggled(event_data, false);
    SAH_TRACEZ_OUT(ME);
    return;
}
