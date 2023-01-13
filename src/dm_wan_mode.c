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

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>

#include "dm_wan-manager.h"
#include "dm_wan_mode.h"
#include "ctrl/restart.h"
#include "netmodel/nm_query.h"
#include "autosensing/autosensing.h"
#include "wan_manager_utils.h"
#include "component.h"
#include "dns/dns.h"

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
    { IPv6_DHCP, "dhcp6" },
    { IPv6_PPP, "ppp6" },
    { IPv6_STATIC, "static" },
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
                                     bool include_type);
static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode);
static mode_ctrl_t wan_mode_convert_from_str(const char* mode, bool ipv4);
static const char* wan_mode_status_to_str(wan_mode_status_t status);
static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status);
static operation_mode_t update_operation_mode(const char* new_operation_mode);
static operation_mode_t startup_wan_autosensing(void);
static char* get_physical_type_for_wan_mode(const char* wan_mode);

static void update_sensing(void) {
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
    return;
}

void wan_mode_init(void) {
    const char* prefix = wan_get_prefix();

    when_null_trace(prefix, exit, ERROR, "Failed to find the prefix");
    wan_manager = amxd_dm_findf(wan_get_dm(), "%sWANManager", prefix);
    when_null_trace(wan_manager, exit, ERROR, "Failed to find the WANManager instance");
    nm_query_ll_init();
exit:
    return;
}

amxd_object_t* get_wan_manager_obj(void) {
    return wan_manager;
}

void wan_mode_cleanup(void) {
    wan_manager = NULL;
}

void wan_manager_found_ll(const char* phys_type) {
    char* current_wan_mode_str = NULL;
    char* physical_type = NULL;
    operation_mode_t operation_mode = startup_wan_autosensing();

    when_null_trace(phys_type, exit, ERROR, "Bad physical type was given");
    current_wan_mode_str = get_current_wan_mode_str();
    when_null_trace(current_wan_mode_str, exit, ERROR, "Failed to get the current wan mode");

    physical_type = get_physical_type_for_wan_mode(current_wan_mode_str);
    when_null_trace(physical_type, exit, ERROR, "Failed to get the physical type for the current wan mode");

    if((strcmp(phys_type, physical_type) == 0) && (operation_mode != OPERATION_MODE_AUTOMATIC)) {
        wan_mode_set(current_wan_mode_str, current_wan_mode_str);
    }
exit:
    free(physical_type);
    free(current_wan_mode_str);
}

static char* get_physical_type_for_wan_mode(const char* wan_mode) {
    amxd_object_t* wan_mode_inst = NULL;
    char* physical_type = NULL;

    when_null_trace(wan_mode, exit, ERROR, "Bad input parameter wan_mode given");
    wan_mode_inst = get_wan_mode(wan_mode);
    when_null_trace(wan_mode_inst, exit, ERROR, "%s is not a valid WAN mode", wan_mode);
    physical_type = amxd_object_get_value(cstring_t, wan_mode_inst, "PhysicalType", NULL);
exit:
    return physical_type;
}

static operation_mode_t startup_wan_autosensing(void) {
    operation_mode_t rv = OPERATION_MODE_UNKNOWN;
    const amxc_var_t* var_operation_mode = NULL;

    when_true(wan_autosensing_can_start, exit);
    when_null_trace(wan_manager, exit, ERROR, "Did not get the wan-manager object yet");

    wan_autosensing_can_start = true;
    var_operation_mode = amxd_object_get_param_value(wan_manager, "OperationMode");
    rv = update_operation_mode(GET_CHAR(var_operation_mode, NULL));

exit:
    return rv;
}

static operation_mode_t update_operation_mode(const char* new_operation_mode) {
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
    return rc;
}

amxd_status_t wan_mode_set(const char* wan_mode_to_set, const char* active_wan_mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* active_wan_mode_obj = NULL;
    amxd_object_t* new_wan_mode_obj = NULL;

    SAH_TRACEZ_INFO(ME, "Change mode: [From = %s, To = %s]", active_wan_mode, wan_mode_to_set);
    active_wan_mode_obj = get_wan_mode(active_wan_mode);
    new_wan_mode_obj = get_wan_mode(wan_mode_to_set);
    when_null_trace(active_wan_mode_obj, exit, ERROR, "Current wanmode object could not be found");
    when_null_trace(new_wan_mode_obj, exit, ERROR, "%s is not a valid WAN mode", wan_mode_to_set);

    rc = wan_mode_enable(active_wan_mode_obj, false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable the previous wan mode");

    rc = wan_mode_dm_set(wan_mode_to_set, NULL);
    when_failed_trace(rc, exit, ERROR, "Failed to set new WANMode '%s' in the datamodel", wan_mode_to_set);
    rc = wan_mode_enable(new_wan_mode_obj, true);
    when_failed_trace(rc, exit, WARNING, "Failed to enable '%s' as WANMode", wan_mode_to_set);

    if(wan_mode_different_physical_type(active_wan_mode_obj, new_wan_mode_obj)) {
        rc = restart();
    }

exit:
    if(rc != amxd_status_ok) {
        wan_mode_set_status(new_wan_mode_obj, WAN_Mode_Error);
    }
    return rc;
}

static amxd_status_t wan_mode_intf_enable(amxd_object_t* interface,
                                          const char* lower_layer,
                                          bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t parameters;
    mode_ctrl_t mode = IP_None;

    amxc_var_init(&parameters);
    when_null_trace(interface, exit, ERROR, "Cannot get Interface object for WANMode");
    SAH_TRACEZ_INFO(ME, "interface %d (%s)", interface->index, interface->name);

    when_failed(amxd_object_get_params(interface, &parameters, amxd_dm_access_private), exit);

    amxc_var_add_key(cstring_t, &parameters, "LowerLayer", lower_layer);

    // Get mode for IPv4 & IPv6
    mode = get_wan_mode_type(interface, true);
    if(IP_None != mode) {
        rc = mode_ctrl_action(mode, &parameters, enable);
    }

exit:
    amxc_var_clean(&parameters);
    return rc;
}

/**
 * @brief Enables or disables a wan mode
 * @param wan_mode The datamodel object for the mode that should be enabled/disabled
 * @param enable Enables the mode if set to true, otherwise it disables the mode
 * @return amxd_status_ok if the mode was enabled/disabled correctly, otherwise it returns an error
 */
amxd_status_t wan_mode_enable(amxd_object_t* wan_mode, bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* lower_layer = NULL;
    char* physical_type = NULL;
    char* dns_mode = NULL;

    when_null_trace(wan_mode, exit, ERROR, "bad wan mode object given");
    if(!enable) {
        wan_mode_set_status(wan_mode, WAN_Mode_Disabled);
    }

    physical_type = amxd_object_get_value(cstring_t, wan_mode, "PhysicalType", NULL);
    lower_layer = nm_query_get_lower_layer(physical_type);
    dns_mode = amxd_object_get_value(cstring_t, wan_mode, "DNSMode", NULL);
    when_str_empty_trace(lower_layer, exit, ERROR, "LowerLayer for PhysicalType %s returned empty (or null)", physical_type);

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        rc = wan_mode_intf_enable(interface, lower_layer, enable);
        when_failed_trace(rc, exit, ERROR, "Failed to enable interface '%s' with code %d", amxd_object_get_name(interface, AMXD_OBJECT_NAMED), rc);
    }

    if(enable) {
        rc = dns_mode_set(wan_mode, dns_mode);
    } else {
        rc = dns_mode_unset(wan_mode, dns_mode);
    }
    when_failed_trace(rc, exit, ERROR, "failed with code %d, unable to %s the DNS mode", rc, enable ? "set" : "unset");

exit:
    free(physical_type);
    free(dns_mode);
    if(enable) {
        wan_mode_set_status(wan_mode, (amxd_status_ok == rc ? WAN_Mode_Enabled : WAN_Mode_Error));
    }
    return rc;
}

/**
 * @brief This function can be used to get datamodel object for a specific mode
 * @param alias The alias of the requested mode
 * @return A pointer to the amxd_object_t for the requested WANMode, NULL if no mode was found with this alias
 */
amxd_object_t* get_wan_mode(const char* alias) {
    amxd_object_t* wan_mode_obj = NULL;
    when_str_empty_trace(alias, exit, ERROR, "Could not find wan mode, no Alias provided");
    wan_mode_obj = amxd_dm_findf(wan_get_dm(), "%sWANManager.WAN.[Alias=='%s'].",
                                 wan_get_prefix(), alias);

exit:
    return wan_mode_obj;
}

/**
 * @brief This function can be used to get the string value contained in the WANMode object
 * @return A string is returned containing the current WANMode, the string must be freed when no longer needed
 */
char* get_current_wan_mode_str(void) {
    char* current_wan_mode_str = NULL;

    when_null(wan_manager, exit);
    current_wan_mode_str = amxd_object_get_value(cstring_t, wan_manager, "WANMode", NULL);

exit:
    return current_wan_mode_str;
}

/**
 * @brief This function can be used to get datamodel object for the mode currently set in the WANMode parameter
 * @return The pointer to the amxd_object_t for the WANMode that is currently configured
 */
amxd_object_t* get_current_wan_mode(void) {
    amxd_object_t* wan_mode_obj = NULL;
    char* current_wan_mode_str = get_current_wan_mode_str();

    when_str_empty_trace(current_wan_mode_str, exit, ERROR, "Failed to get the current wanmode");

    wan_mode_obj = get_wan_mode(current_wan_mode_str);
    when_null_trace(wan_mode_obj, exit, ERROR, "Cannot get current WANMode object");

exit:
    free(current_wan_mode_str);
    return wan_mode_obj;
}

static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode) {
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
    return rc;
}

static const char* wan_mode_status_to_str(wan_mode_status_t status) {
    return ((WAN_Mode_Enabled <= status) && (WAN_Mode_Nr_ > status))
           ? wan_mode_status_str[status] : wan_mode_status_str[WAN_Mode_Error];
}

static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status) {
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
    return rc;
}

mode_ctrl_t wan_mode_convert_from_str(const char* mode, bool ipv4) {
    mode_cnv_t* lookup = mode_cnv;
    mode_ctrl_t rc = IP_None;
    while(lookup->str != NULL) {
        if(0 == strcmp(mode, lookup->str)) {
            rc = lookup->mode;
            // keyword static is used in both parameter IPv4Mode & IPv6Mode
            if((rc == IPv4_STATIC) && (ipv4 == false)) {
                rc = IPv6_STATIC;
            }
            break;
        }
        lookup++;
    }
    return rc;
}

static mode_ctrl_t get_wan_mode_type(amxd_object_t* interface,
                                     bool include_type) {
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

    ipv4mode = GET_CHAR(&data, "IPv4Mode");
    when_str_empty_trace(ipv4mode, exit, ERROR, "Empty 'IPv4Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv4mode, true);

    ipv6mode = GET_CHAR(&data, "IPv6Mode");
    when_str_empty_trace(ipv6mode, exit, ERROR, "Empty 'IPv6Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv6mode, false);

    SAH_TRACEZ_INFO(ME, "Type '%s', ipv4 '%s', ipv6 '%s': mode %#06X",
                    include_type ? type : "", ipv4mode, ipv6mode, mode);

exit:
    amxc_var_clean(&data);
    return (mode_ctrl_t) mode;
}

void _update_autosensing(UNUSED const char* const event_name,
                         const amxc_var_t* const event_data,
                         UNUSED void* const priv) {
    SAH_TRACEZ_INFO(ME, "Toggling OperationMode from %s to %s",
                    GETP_CHAR(event_data, "parameters.OperationMode.from"),
                    GETP_CHAR(event_data, "parameters.OperationMode.to"));
    update_operation_mode(GETP_CHAR(event_data, "parameters.OperationMode.to"));
}

void _update_sensing_policy(UNUSED const char* const event_name,
                            UNUSED const amxc_var_t* const event_data,
                            UNUSED void* const priv) {
    update_sensing();
}

void _wan_sensing_toggled(UNUSED const char* const event_name,
                          UNUSED const amxc_var_t* const event_data,
                          UNUSED void* const priv) {
    update_sensing();
}
