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
static bool wan_autosensing_was_started = false;

static mode_ctrl_t get_wan_mode_type(amxd_object_t* interface,
                                     bool include_type);
static bool wan_mode_different_physical_type(amxd_object_t* const current, amxd_object_t* const new_mode);
static mode_ctrl_t wan_mode_convert_from_str(const char* mode, bool ipv4);
static const char* wan_mode_status_to_str(wan_mode_status_t status);
static amxd_status_t wan_mode_set_status(amxd_object_t* const object, wan_mode_status_t status);
static void update_operation_mode(const char* new_operation_mode);
static void startup_wan_autosensing(void);
static char* get_physical_type_for_wan_mode(const char* wan_mode);

void wan_mode_init(void) {
    const char* prefix = wan_get_prefix();

    when_null_trace(prefix, exit, ERROR, "Failed to find the prefix");
    wan_manager = amxd_dm_findf(wan_get_dm(), "%sWANManager", prefix);
    when_null_trace(wan_manager, exit, ERROR, "Failed to find the WANManager instance");
    nm_query_ll_init();
exit:
    return;
}

void wan_mode_cleanup(void) {
    wan_manager = NULL;
}

void wan_manager_found_ll(const char* phys_type) {
    char* current_wan_mode_str = NULL;
    char* physical_type = NULL;

    startup_wan_autosensing();

    //Check if the current wanmode needs this information
    when_null_trace(wan_manager, exit, ERROR, "Did not get the wan-manager object yet")
    when_null_trace(phys_type, exit, ERROR, "Bad phys type was given");
    current_wan_mode_str = amxd_object_get_value(cstring_t, wan_manager, "WANMode", NULL);
    when_null_trace(current_wan_mode_str, exit, ERROR, "Failed to get the current wan mode");

    physical_type = get_physical_type_for_wan_mode(current_wan_mode_str);
    when_null_trace(physical_type, exit, ERROR, "Failed to get the physical type for the current wan mode");
    if(strcmp(phys_type, physical_type) == 0) {
        wan_mode_set(current_wan_mode_str, "");
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

static void startup_wan_autosensing(void) {
    char* current_operation_mode = NULL;

    when_true(wan_autosensing_was_started, exit);
    when_null_trace(wan_manager, exit, ERROR, "Did not get the wan-manager object yet")

    current_operation_mode = amxd_object_get_value(cstring_t, wan_manager, "OperationMode", NULL);
    update_operation_mode(current_operation_mode);
    free(current_operation_mode);
    wan_autosensing_was_started = true;
exit:
    return;
}

static void update_operation_mode(const char* new_operation_mode) {
    when_str_empty_trace(new_operation_mode, exit, ERROR, "Bad new operation mode value");

    if(0 == strcmp(new_operation_mode, "Automatic")) {
        SAH_TRACEZ_INFO(ME, "WANManager set to automatic mode enable WANAutosensing");
        autosensing_set_enable(true);
    } else if(0 == strcmp(new_operation_mode, "Manual")) {
        SAH_TRACEZ_INFO(ME, "WANManager set to manual mode disable WANAutosensing");
        autosensing_set_enable(false);
    } else {
        SAH_TRACEZ_ERROR(ME, "Unsupported operation mode[%s]", new_operation_mode);
    }
exit:
    return;
}

amxd_status_t wan_mode_dm_set(const char* value) {
    amxd_status_t rc = amxd_status_unknown_error;
    when_null(value, exit);

    rc = amxd_object_set_value(cstring_t, wan_manager, "WANMode", value);
exit:
    return rc;
}

amxd_status_t wan_mode_set(const char* wan_mode_to_set, const char* current_wan_mode_str) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* current_wan_mode = NULL;
    amxd_object_t* new_wan_mode = NULL;
    size_t len = 0;

    when_null(wan_manager, exit);
    new_wan_mode = get_wan_mode(wan_mode_to_set);
    when_null_trace(new_wan_mode, exit, ERROR, "%s is not a valid WAN mode", wan_mode_to_set);

    if(NULL != current_wan_mode_str) {
        if(0 == strcmp(wan_mode_to_set, current_wan_mode_str)) {
            SAH_TRACEZ_INFO(ME, "%s WAN mode is already configured", wan_mode_to_set);
            rc = wan_mode_set_status(new_wan_mode, WAN_Mode_Enabled);
            goto exit;
        }

        current_wan_mode = get_wan_mode(current_wan_mode_str);
        len = strlen(current_wan_mode_str);
    }
    when_true(((NULL == current_wan_mode) && (0 != len)), exit);
    if((NULL != current_wan_mode) && (wan_mode_disable(current_wan_mode) != amxd_status_ok)) {
        SAH_TRACEZ_ERROR(ME, "Failed to disable the previous wan mode");
    }

    wan_mode_dm_set(wan_mode_to_set);
    when_failed_trace((rc = wan_mode_enable(new_wan_mode)), exit, WARNING,
                      "WAN mode enable error [WANMode=%s] -> should be added after netmodel cb", wan_mode_to_set);

    if(wan_mode_different_physical_type(current_wan_mode, new_wan_mode)) {
        rc = restart();
    }
exit:
    return rc;
}

static amxd_status_t wan_mode_intf_disable(amxd_object_t* interface,
                                           const char* lower_layer) {
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
        rc = mode_ctrl_action(mode, &parameters, false);
    }
exit:
    amxc_var_clean(&parameters);
    return rc;
}

amxd_status_t wan_mode_disable(amxd_object_t* wan_mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* lower_layer = NULL;
    const char* dns_mode = NULL;
    char* physical_type = NULL;

    when_null(wan_mode, exit);
    (void) wan_mode_set_status(wan_mode, WAN_Mode_Disabled);

    physical_type = amxd_object_get_value(cstring_t, wan_mode, "PhysicalType", NULL);
    lower_layer = nm_query_get_lower_layer(physical_type);
    dns_mode = GET_CHAR(amxd_object_get_param_value(wan_mode, "DNSMode"), NULL);
    when_str_empty_trace(lower_layer, exit, ERROR, "LowerLayer for PhysicalType %s" \
                         " returned empty (or null)", physical_type);

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        rc = wan_mode_intf_disable(interface, lower_layer);
        when_failed_trace(rc, exit, ERROR, "failed with code %d", rc);
    }

    rc = dns_mode_unset(wan_mode, dns_mode);
    when_failed_trace(rc, exit, ERROR, "failed with code %d, unable to unset the DNS mode", rc);

exit:
    free(physical_type);
    return rc;
}

static amxd_status_t wan_mode_intf_enable(amxd_object_t* interface,
                                          const char* lower_layer) {
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
        rc = mode_ctrl_action(mode, &parameters, true);
    }

exit:
    amxc_var_clean(&parameters);
    return rc;
}

amxd_status_t wan_mode_enable(amxd_object_t* wan_mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* lower_layer = NULL;
    char* physical_type = NULL;
    char* dns_mode = NULL;

    when_null_trace(wan_mode, exit, ERROR, "bad wan mode object given");
    physical_type = amxd_object_get_value(cstring_t, wan_mode, "PhysicalType", NULL);
    lower_layer = nm_query_get_lower_layer(physical_type);
    dns_mode = amxd_object_get_value(cstring_t, wan_mode, "DNSMode", NULL);
    when_str_empty_trace(lower_layer, exit, WARNING, "LowerLayer for PhysicalType %s" \
                         " returned empty (or null) -> should be added after netmodel cb returns", physical_type);

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        rc = wan_mode_intf_enable(interface, lower_layer);
        when_failed_trace(rc, exit, ERROR, "failed with code %d", rc);
    }

    rc = dns_mode_set(wan_mode, dns_mode);
    when_failed_trace(rc, exit, ERROR, "failed with code %d, unable to set the DNS mode", rc);

exit:
    free(physical_type);
    free(dns_mode);
    wan_mode_set_status(wan_mode, (amxd_status_ok == rc ? WAN_Mode_Enabled : WAN_Mode_Error));
    return rc;
}

amxd_object_t* get_wan_mode(const char* alias) {
    return amxd_dm_findf(wan_get_dm(), "%sWANManager.WAN.[Alias=='%s'].",
                         wan_get_prefix(), alias);
}

amxc_string_t* wan_mode_get_interface(void) {
    char* current_wan_mode_str = NULL;
    char* ip_ref = NULL;
    amxd_object_t* wan_mode = NULL;
    amxd_object_t* interface_tmpl = NULL;
    amxd_object_t* interface = NULL;
    amxc_string_t* interface_name = NULL;

    when_null(wan_manager, exit);
    current_wan_mode_str = amxd_object_get_value(cstring_t, wan_manager, "WANMode", NULL);

    wan_mode = get_wan_mode(current_wan_mode_str);
    when_null_trace(wan_mode, exit, ERROR, "Cannot get current WANMode object");

    interface_tmpl = amxd_object_get_child(wan_mode, "Intf");
    when_null_trace(interface_tmpl, exit, ERROR, "Cannot get Interface template object for WANMode");

    interface = amxd_object_get_instance(interface_tmpl, NULL, 1);
    when_null_trace(interface, exit, ERROR, "Cannot get Interface object for WANMode");

    ip_ref = amxd_object_get_value(cstring_t, interface, "IPv4Reference", NULL);
    amxc_string_new(&interface_name, 0);
    amxc_string_setf(interface_name, "%sIPv4Address.", ip_ref);

exit:
    free(current_wan_mode_str);
    free(ip_ref);
    return interface_name;
}

mode_ctrl_t wan_mode_get_mode(void) {
    char* current_wan_mode_str = NULL;
    char* type = NULL;
    mode_ctrl_t rc = IP_None;
    amxd_object_t* wan_mode = NULL;
    amxd_object_t* interface_tmpl = NULL;
    amxd_object_t* interface = NULL;

    when_null(wan_manager, exit);
    current_wan_mode_str = amxd_object_get_value(cstring_t, wan_manager, "WANMode", NULL);

    wan_mode = get_wan_mode(current_wan_mode_str);
    when_null_trace(wan_mode, exit, ERROR, "Cannot get current WANMode object");

    interface_tmpl = amxd_object_get_child(wan_mode, "Intf");
    when_null_trace(interface_tmpl, exit, ERROR, "Cannot get Interface template object for WANMode");

    interface = amxd_object_get_instance(interface_tmpl, NULL, 1);
    when_null_trace(interface, exit, ERROR, "Cannot get Interface object for WANMode");

    type = amxd_object_get_value(cstring_t, interface, "IPv4Mode", NULL);
    rc = get_wan_mode_type(interface, false);

exit:
    free(current_wan_mode_str);
    free(type);
    return rc;
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
    amxc_var_t status_parameter;

    when_null(object, exit);
    when_failed(amxc_var_init(&status_parameter), exit);
    when_failed(amxc_var_set_type(&status_parameter, AMXC_VAR_ID_CSTRING), exit);
    when_failed(amxc_var_set(cstring_t, &status_parameter, wan_mode_status_to_str(status)), exit);

    rc = amxd_object_set_param(object, "Status", &status_parameter);

exit:
    amxc_var_clean(&status_parameter);
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
    char* type = NULL;
    char* ipv4mode = NULL;
    char* ipv6mode = NULL;
    int mode = (int) IP_None;

    if(include_type == true) {
        type = amxd_object_get_value(cstring_t, interface, "Type", NULL);
        when_str_empty_trace(type, exit, ERROR, "Empty 'Type' parameter");
        mode = (int) wan_mode_convert_from_str(type, false);
    }

    ipv4mode = amxd_object_get_value(cstring_t, interface, "IPv4Mode", NULL);
    when_str_empty_trace(ipv4mode, exit, ERROR, "Empty 'IPv4Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv4mode, true);

    ipv6mode = amxd_object_get_value(cstring_t, interface, "IPv6Mode", NULL);
    when_str_empty_trace(ipv6mode, exit, ERROR, "Empty 'IPv6Mode' parameter");
    mode |= (int) wan_mode_convert_from_str(ipv6mode, false);

exit:
    SAH_TRACEZ_INFO(ME, "Type '%s', ipv4 '%s', ipv6 '%s': mode 0x%06X",
                    include_type ? type : "", ipv4mode, ipv6mode, mode);
    free(type);
    free(ipv4mode);
    free(ipv6mode);
    return (mode_ctrl_t) mode;
}

void _update_autosensing(UNUSED const char* const event_name,
                         const amxc_var_t* const event_data,
                         UNUSED void* const priv) {
    update_operation_mode(GETP_CHAR(event_data, "parameters.OperationMode.to"));
}