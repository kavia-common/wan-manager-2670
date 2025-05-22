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

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>

#include "dm_wan_mode.h"
#include "netmodel/nm_query.h"
#include "autosensing/autosensing.h"
#include "upstream_intf.h"
#include "wan_manager_utils.h"

#define LOGICAL4_UP_FLAG "logical4-up"
#define LOGICAL6_UP_FLAG "logical6-up"
#define LOGICAL_UP_FLAGS LOGICAL4_UP_FLAG " " LOGICAL6_UP_FLAG

#define ME "netmod-ctrl"

void ll_queries_clean(nm_query_ll_info_t** info) {
    when_null(*info, exit);

    netmodel_closeQuery((*info)->q_name);
    netmodel_closeQuery((*info)->q_intf_path);
    netmodel_closeQuery((*info)->q_phys_up);
    free((*info)->intf_name);
    free((*info)->lower_layer);
    free((*info)->upstream_intf_path);
    free(*info);
    *info = NULL;

exit:
    return;
}

static void nm_query_response_phys_up_cb(UNUSED const char* sig_name,
                                         const amxc_var_t* data,
                                         void* priv) {
    SAH_TRACEZ_IN(ME);
    const char* physical_reference = (const char*) priv;
    bool up = false;

    when_str_empty(physical_reference, exit);

    up = GET_BOOL(data, NULL);
    SAH_TRACEZ_INFO(ME, "Physical interface %s is %s", physical_reference, up ? "UP" : "DOWN");

    // Not needed if physical ref is used for current wanmode.
    // We have the ipv4 and ipv6 up queries for these cases.
    if(!physical_reference_used(physical_reference)) {
        mod_autosensing_notify_intf_changed(physical_reference, up, is_autosensing_enabled());
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

void nm_query_create_phys_up_query(nm_query_ll_info_t* info) {
    SAH_TRACEZ_IN(ME);

    when_null_trace(info, exit, ERROR, "Invalid private data");
    when_str_empty_trace(info->upstream_intf_path, exit, ERROR, "Invalid physical interface");

    netmodel_closeQuery(info->q_phys_up);
    info->q_phys_up = netmodel_openQuery_isUp(info->upstream_intf_path, "wan-manager", "", netmodel_traverse_this, nm_query_response_phys_up_cb, (void*) info->upstream_intf_path);
    when_null_trace(info->q_phys_up, exit, ERROR, "Could not open NetModel query");

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

static void nm_query_response_ll_cb(UNUSED const char* sig_name,
                                    const amxc_var_t* data,
                                    void* priv) {
    SAH_TRACEZ_IN(ME);
    nm_query_ll_info_t* info = (nm_query_ll_info_t*) priv;
    const char* lower_layer = NULL;

    when_null_trace(info, exit, ERROR, "private data is null");

    lower_layer = GET_CHAR(data, NULL);
    SAH_TRACEZ_INFO(ME, "LowerLayer query for PhysicalType = %s -> %s", physical_type_to_string(info->physical_type), lower_layer);
    when_str_empty(lower_layer, exit);
    free(info->lower_layer);
    info->lower_layer = trim_final_dot(lower_layer);
    wan_manager_found_ll(info->physical_type);
exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

static void nm_query_create_ll_query(const char* intf_path, nm_query_ll_info_t* info) {
    SAH_TRACEZ_IN(ME);

    when_null_trace(info, exit, ERROR, "Invalid private data");

    netmodel_closeQuery(info->q_intf_path);
    info->q_intf_path = netmodel_openQuery_getFirstParameter(intf_path,
                                                             "wan-manager", "InterfacePath", "",
                                                             netmodel_traverse_one_level_up,
                                                             nm_query_response_ll_cb, (void*) info);
    when_null_trace(info->q_intf_path, exit, ERROR, "Could not open NetModel query");

exit:
    SAH_TRACEZ_OUT(ME);
}

static void nm_query_response_name_cb(UNUSED const char* sig_name,
                                      const amxc_var_t* data,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);
    amxc_string_t intf_path;
    nm_query_ll_info_t* info = (nm_query_ll_info_t*) priv;
    const char* name = GETI_CHAR(data, 0);
    amxc_var_t* var_intf_path = NULL;

    amxc_string_init(&intf_path, 0);
    when_null_trace(info, exit, ERROR, "private data is null");
    when_str_empty(name, exit);

    SAH_TRACEZ_INFO(ME, "Name query for PhysicalType = %s -> %s", physical_type_to_string(info->physical_type), name);

    if((info->intf_name != NULL) && (strcmp(info->intf_name, name) == 0)) {
        goto exit;
    }
    free(info->intf_name);
    info->intf_name = strdup(name);

    // create another query to get the LowerLayer
    amxc_string_setf(&intf_path, "NetModel.Intf.%s.", name);
    var_intf_path = netmodel_getFirstParameter(amxc_string_get(&intf_path, 0), "InterfacePath", "", netmodel_traverse_this);
    free(info->upstream_intf_path);
    info->upstream_intf_path = NULL;
    if(var_intf_path != NULL) {
        info->upstream_intf_path = amxc_var_dyncast(cstring_t, var_intf_path);
    }

    nm_query_create_ll_query(amxc_string_get(&intf_path, 0), info);

exit:
    amxc_var_delete(&var_intf_path);
    amxc_string_clean(&intf_path);
    SAH_TRACEZ_OUT(ME);
    return;
}

/**
 * @brief This function loops over all Interfaces of a WANMode and checks if all active netmodel queries are up.
 *
 * @param intf_obj WANManager.WAN.{}.Intf object
 * @return true when all queries for intf_obj and it's sibling Intf objects are returning true
 * @return false otherwise
 */
static bool wan_mode_up(amxd_object_t* wan_mode_obj) {
    bool up = false;
    nm_query_ll_info_t* info = NULL;

    when_null_trace(wan_mode_obj, exit, ERROR, "Could not get the wanmode object");
    info = (nm_query_ll_info_t*) wan_mode_obj->priv;
    when_null_trace(info, exit, ERROR, "Could not find wanmode private data");

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode_obj, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        intf_isup_queries_t* nm_queries = NULL;
        when_null_trace(interface, exit, ERROR, "Could not get interface object");
        nm_queries = (intf_isup_queries_t*) interface->priv;
        when_null_trace(nm_queries, exit, ERROR, "Could not get netmodel queries");

        when_true(nm_queries->ipv4_needed && nm_queries->ipv4_result == false, exit);
        when_true(nm_queries->ipv6_needed && nm_queries->ipv6_result == false, exit);
    }

    up = true;

exit:
    SAH_TRACEZ_INFO(ME, "WANMode is %s!", up ? "UP" : "(still) DOWN");
    if(info != NULL) {
        info->wan_mode_up = up;
    }
    return up;
}

/**
 * @brief This function will handle the netmodel flags and, if required, stopping autosensing
 * @param data a variant that contains a boolean indication if the mode interface is active or not
 * @param intf_obj The wan mode interface object for which the flag will be toggled on the corresponding logical netmodel interface
 * @param flag The flag that should be toggled
 * @param ip_version Indicates whether IPv4 or IPv6 mode interface is active or not
 */
static void nm_query_mode_active_handle_flags(const amxc_var_t* data, amxd_object_t* intf_obj, const char* flag, ipversion_t ip_version) {
    SAH_TRACEZ_IN(ME);
    bool wan_mode_active = false;
    bool intf_was_up_before = false;
    bool active = GET_BOOL(data, NULL);
    const char* intf_name = object_const_string(intf_obj, "Name");
    nm_query_ll_info_t* info = NULL;
    intf_isup_queries_t* nm_queries = NULL;
    amxd_object_t* wan_mode_obj = NULL;

    when_null_trace(intf_obj, exit, ERROR, "Failed to get interface object");
    when_str_empty_trace(intf_name, exit, ERROR, "Failed to get interface name");

    wan_mode_obj = amxd_object_get_parent(amxd_object_get_parent(intf_obj));
    when_null_trace(wan_mode_obj, exit, ERROR, "Could not get the wanmode object");
    info = (nm_query_ll_info_t*) wan_mode_obj->priv;
    when_null_trace(info, exit, ERROR, "Failed to get netmodel queries");
    intf_was_up_before = info->wan_mode_up;

    nm_queries = (intf_isup_queries_t*) intf_obj->priv;
    when_null_trace(nm_queries, exit, ERROR, "Failed to get netmodel queries");
    if(ip_version == IPv4) {
        nm_queries->ipv4_result = active;
    } else if(ip_version == IPv6) {
        nm_queries->ipv6_result = active;
    }

    if(active) {
        SAH_TRACEZ_INFO(ME, "Setting flag '%s' on interface '%s'", flag, intf_name);
        netmodel_setFlag(intf_name, flag, NULL, netmodel_traverse_this);
    } else {
        SAH_TRACEZ_INFO(ME, "Clearing flag '%s' from interface '%s'", flag, intf_name);
        netmodel_clearFlag(intf_name, flag, NULL, netmodel_traverse_this);
    }

    wan_mode_active = wan_mode_up(wan_mode_obj);
    if(wan_mode_active) {
        autosensing_found_mode();
        mod_autosensing_stop();
    } else {
        if(intf_was_up_before) {
            if(!str_empty(info->upstream_intf_path)) {
                /* restart happens with mod_autosensing start below*/
                mod_autosensing_notify_intf_changed(info->upstream_intf_path, false, false);
            }
            if(is_autosensing_enabled()) {
                mod_autosensing_start();
            }
        }
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

static void nm_query_mode_active_cb(UNUSED const char* sig_name,
                                    const amxc_var_t* data,
                                    void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* intf_obj = (amxd_object_t*) priv;

    nm_query_mode_active_handle_flags(data, intf_obj, LOGICAL4_UP_FLAG, IPv4);
    SAH_TRACEZ_OUT(ME);
}

static void nm_query_mode6_active_cb(UNUSED const char* sig_name,
                                     const amxc_var_t* data,
                                     void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* intf_obj = (amxd_object_t*) priv;

    nm_query_mode_active_handle_flags(data, intf_obj, LOGICAL6_UP_FLAG, IPv6);
    SAH_TRACEZ_OUT(ME);
}

static int nm_query_create_name_query(amxd_object_t* wan_mode,
                                      nm_query_ll_info_t* info,
                                      const char* flag) {
    SAH_TRACEZ_IN(ME);
    int rv = -2;
    amxc_string_t str_flags;
    amxc_string_init(&str_flags, 0);

    when_null_trace(wan_mode, exit, ERROR, "Could not find wan-mode");

    amxc_string_setf(&str_flags, "%s && upstream", flag);
    info->q_name = netmodel_openQuery_getIntfs("NetModel.Intf.resolver.", "wan-manager",
                                               amxc_string_get(&str_flags, 0),
                                               netmodel_traverse_all,
                                               nm_query_response_name_cb, (void*) info);
    if(info->q_name != NULL) {
        SAH_TRACEZ_INFO(ME, "Query getIntfs '%s' succeeded", amxc_string_get(&str_flags, 0));
        rv = 0;
    } else {
        SAH_TRACEZ_ERROR(ME, "Query getIntfs '%s' failed", amxc_string_get(&str_flags, 0));
    }

exit:
    amxc_string_clean(&str_flags);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

int nm_query_ll_add(amxd_object_t* wan_mode) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    physical_type_t physical_type = get_physical_type(wan_mode);
    const char* physical_reference = object_const_string(wan_mode, "PhysicalReference");
    nm_query_ll_info_t* info = NULL;

    when_null_trace(wan_mode, exit, ERROR, "Skipping empty WANMode");
    info = (nm_query_ll_info_t*) calloc(1, sizeof(nm_query_ll_info_t));
    when_null_trace(info, exit, ERROR, "Failed to allocate memory for queries");

    if(wan_mode->priv != NULL) {
        SAH_TRACEZ_ERROR(ME, "WANMode already has a query, closing old query");
        nm_query_ll_info_t* old_nm_queries = (nm_query_ll_info_t*) wan_mode->priv;
        ll_queries_clean(&old_nm_queries);
    }
    wan_mode->priv = info;
    info->wan_mode_up = false;
    info->physical_type = physical_type;

    if(str_empty(physical_reference)) {
        const char* phys_type_flag = phys_type_to_flag(info->physical_type);

        when_null_trace(phys_type_flag, exit, WARNING, "Query flag for PhysicalType is not yet defined [index %d]", info->physical_type);
        SAH_TRACEZ_INFO(ME, "Create query for PhysicalType '%s' [index %d]", physical_type_to_string(physical_type), info->physical_type);

        rv = nm_query_create_name_query(wan_mode, info, phys_type_flag);
        when_failed_trace(rv, exit, ERROR, "Query for %s failed", physical_type_to_string(physical_type));
    } else {
        free(info->upstream_intf_path);
        info->upstream_intf_path = strdup(physical_reference);

        nm_query_create_ll_query(physical_reference, info);
        rv = 0;
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

void intf_isup_queries_clean(intf_isup_queries_t** nm_queries) {
    SAH_TRACEZ_IN(ME);
    if((nm_queries == NULL) || (*nm_queries == NULL)) {
        goto exit;
    }

    netmodel_closeQuery((*nm_queries)->nm_ipv4_up_query);
    netmodel_closeQuery((*nm_queries)->nm_ipv6_up_query);
    free(*nm_queries);
    *nm_queries = NULL;

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

static void init_intf_isup_query(amxd_object_t* interface, intf_isup_queries_t* nm_queries, ipversion_t ip_version) {
    amxc_var_t intf_params;
    const char* ip_reference_path = NULL;
    bool* query_needed = NULL;

    amxc_var_init(&intf_params);
    amxc_var_set_type(&intf_params, AMXC_VAR_ID_HTABLE);

    when_null(nm_queries, exit);

    amxd_object_get_params(interface, &intf_params, amxd_dm_access_protected);
    ip_reference_path = (ip_version == IPv4) ? GET_CHAR(&intf_params, IPV4_REFERENCE_PATH) : GET_CHAR(&intf_params, IPV6_REFERENCE_PATH);

    query_needed = (ip_version == IPv4) ? &(nm_queries->ipv4_needed) : &(nm_queries->ipv6_needed);
    *query_needed = (wan_mode_convert_from_str(GET_CHAR(&intf_params, (ip_version == IPv4 ? "IPv4Mode" : "IPv6Mode")), ip_version) != IP_None) && (!str_empty(ip_reference_path));

exit:
    amxc_var_clean(&intf_params);
}

static int open_intf_isup_query(amxd_object_t* interface, netmodel_callback_t handler, ipversion_t ip_version) {
    intf_isup_queries_t* nm_queries = (intf_isup_queries_t*) interface->priv;
    netmodel_query_t** up_query = NULL;
    char* ip_reference_path = NULL;
    int rv = -1;

    when_null(nm_queries, exit);
    when_false_status(ip_version == IPv4 ? nm_queries->ipv4_needed : nm_queries->ipv6_needed, exit, rv = 0);

    ip_reference_path = amxd_object_get_cstring_t(interface, ip_version == IPv4 ? IPV4_REFERENCE_PATH : IPV6_REFERENCE_PATH, NULL);
    when_str_empty(ip_reference_path, exit);

    up_query = (ip_version == IPv4) ? &(nm_queries->nm_ipv4_up_query) : &(nm_queries->nm_ipv6_up_query);
    *up_query = netmodel_openQuery_isUp(ip_reference_path, "wan-manager", (ip_version == IPv4) ? "ipv4-up" : "ipv6-up", netmodel_traverse_this, handler, interface);
    when_null_trace(*up_query, exit, ERROR, "Could not open IPv%d up query", ip_version);

    rv = 0;

exit:
    free(ip_reference_path);
    return rv;
}


/**
 * @brief Create a netmodel query that will monitor if the modes is functional
 * @return Returns 0 is all queries where created successful, -1 otherwise
 */
int nm_query_mode_active(void) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    const char* wan_mode_str = get_current_wan_mode_str();
    amxc_string_t current_wan_modes_str;
    amxc_llist_t current_list;

    amxc_string_init(&current_wan_modes_str, 0);
    amxc_llist_init(&current_list);

    amxc_string_set(&current_wan_modes_str, wan_mode_str);
    amxc_string_split_to_llist(&current_wan_modes_str, &current_list, ',');
    amxc_llist_for_each(mode_it, &current_list) {
        // Loop over all active WANModes
        const char* wan_mode = amxc_string_get(amxc_string_from_llist_it(mode_it), 0);
        amxd_object_t* wan_mode_obj = get_wan_mode(wan_mode);
        when_null_trace(wan_mode_obj, exit, ERROR, "Failed to start query, no wan mode found");

        amxd_object_for_each(instance, it, amxd_object_findf(wan_mode_obj, ".Intf.")) {
            // Loop over all Intf objects of an active WANMode
            amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
            intf_isup_queries_t* nm_queries = NULL;

            when_null_trace(interface, exit_loop, ERROR, "No interface found to open query");

            if(interface->priv != NULL) {
                SAH_TRACEZ_ERROR(ME, "Interface already has a query, closing old query");
                intf_isup_queries_t* old_nm_queries = (intf_isup_queries_t*) interface->priv;
                intf_isup_queries_clean(&old_nm_queries);
                interface->priv = NULL;
            }

            SAH_TRACEZ_INFO(ME, "Adding queries for '%s'", interface->name);
            nm_queries = (intf_isup_queries_t*) calloc(1, sizeof(intf_isup_queries_t));
            when_null_trace(nm_queries, exit_loop, ERROR, "Failed to allocate memory for queries");
            interface->priv = nm_queries;
            init_intf_isup_query(interface, nm_queries, IPv4);
            init_intf_isup_query(interface, nm_queries, IPv6);

            // The queries cannot be opened in "init_intf_isup_query" itself!
            // We need to know the values of nm_queries->ipv4_needed and nm_queries->ipv6_needed first since they are used in the callback functions of the queries!
            if((open_intf_isup_query(interface, nm_query_mode_active_cb, IPv4) != 0) || (open_intf_isup_query(interface, nm_query_mode6_active_cb, IPv6) != 0)) {
                intf_isup_queries_clean(&nm_queries);
                interface->priv = NULL;
            }

exit_loop:
            (void) 0;
        }
    }
    rv = 0;

exit:
    amxc_llist_clean(&current_list, amxc_string_list_it_free);
    amxc_string_clean(&current_wan_modes_str);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

void nm_close_sensing_queries(void) {
    SAH_TRACEZ_IN(ME);
    const char* wan_mode_str = get_current_wan_mode_str();
    amxc_string_t current_wan_modes_str;
    amxc_llist_t current_list;

    amxc_string_init(&current_wan_modes_str, 0);
    amxc_llist_init(&current_list);

    SAH_TRACEZ_INFO(ME, "Stopping all queries on current wan modes");
    when_str_empty_trace(wan_mode_str, exit, ERROR, "Failed to stop queries, no wan modes found");

    amxc_string_set(&current_wan_modes_str, wan_mode_str);
    amxc_string_split_to_llist(&current_wan_modes_str, &current_list, ',');
    amxc_llist_for_each(mode_it, &current_list) {
        const char* wan_mode = amxc_string_get(amxc_string_from_llist_it(mode_it), 0);
        amxd_object_t* wan_mode_obj = get_wan_mode(wan_mode);
        when_null_trace(wan_mode_obj, exit, ERROR, "Cannot get current WANMode object");
        amxd_object_for_each(instance, intf_it, amxd_object_findf(wan_mode_obj, ".Intf.")) {
            amxd_object_t* interface = amxc_container_of(intf_it, amxd_object_t, it);
            const char* intf_name = object_const_string(interface, "Name");
            intf_isup_queries_t* nm_queries = (intf_isup_queries_t*) interface->priv;
            intf_isup_queries_clean(&nm_queries);
            interface->priv = NULL;
            SAH_TRACEZ_INFO(ME, "Clearing queries from '%s'", interface->name);
            netmodel_clearFlag(intf_name, LOGICAL_UP_FLAGS, NULL, netmodel_traverse_this);
        }
    }

exit:
    amxc_llist_clean(&current_list, amxc_string_list_it_free);
    amxc_string_clean(&current_wan_modes_str);
    SAH_TRACEZ_OUT(ME);
    return;
}
