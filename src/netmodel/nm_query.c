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
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <netmodel/client.h>
#include <netmodel/common_api.h>

#include "dm_wan_mode.h"
#include "netmodel/nm_query.h"
#include "autosensing/autosensing.h"
#include "upstream_intf.h"
#include "wan_manager_utils.h"

#define LOGICAL4_UP_FLAG "logical4-up"
#define LOGICAL6_UP_FLAG "logical6-up"
#define LOGICAL_UP_FLAGS LOGICAL4_UP_FLAG " " LOGICAL6_UP_FLAG

#define ME "netmod-ctrl"

nm_query_ll_info_t ll_info[physical_type_last];

void nm_query_ll_init(void) {
    SAH_TRACEZ_IN(ME);
    memset((void*) ll_info, 0, sizeof(ll_info));
    SAH_TRACEZ_OUT(ME);
}

void nm_query_ll_cleanup(void) {
    SAH_TRACEZ_IN(ME);
    nm_query_ll_info_t* info = ll_info;
    for(int cnt = 0; cnt < physical_type_last; cnt++, info++) {
        if(info->used) {
            netmodel_closeQuery(info->q_name);
            netmodel_closeQuery(info->q_intf_path);
            free(info->intf_name);
            free(info->lower_layer);
            free(info->upstream_intf_path);
        }
    }
    SAH_TRACEZ_OUT(ME);
}

static void nm_query_response_ll_cb(UNUSED const char* sig_name,
                                    const amxc_var_t* data,
                                    void* priv) {
    SAH_TRACEZ_IN(ME);
    nm_query_ll_info_t* info = (nm_query_ll_info_t*) priv;
    const char* lower_layer = NULL;
    const char* phys_type = NULL;

    when_null_trace(info, exit, ERROR, "private data is null");
    when_true(((info->index < 0) || (info->index >= physical_type_last)), exit);

    lower_layer = GET_CHAR(data, NULL);
    phys_type = index_to_phys_type(info->index);
    SAH_TRACEZ_INFO(ME, "LowerLayer query for PhysicalType = %s -> %s",
                    phys_type, lower_layer);
    when_str_empty(lower_layer, exit);
    free(info->lower_layer);
    info->lower_layer = strdup(lower_layer);
    wan_manager_found_ll(phys_type);
exit:
    SAH_TRACEZ_OUT(ME);
    return;
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
    when_true(((info->index < 0) || (info->index >= physical_type_last)), exit);
    when_str_empty(name, exit);

    SAH_TRACEZ_INFO(ME, "Name query for PhysicalType = %s -> %s", index_to_phys_type(info->index), name);

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

    netmodel_closeQuery(info->q_intf_path);
    info->q_intf_path = netmodel_openQuery_getFirstParameter(amxc_string_get(&intf_path, 0),
                                                             "wan-manager", "InterfacePath", "",
                                                             netmodel_traverse_one_level_up,
                                                             nm_query_response_ll_cb, priv);
exit:
    amxc_var_delete(&var_intf_path);
    amxc_string_clean(&intf_path);
    SAH_TRACEZ_OUT(ME);
    return;
}


/**
 * @brief This function will handle the netmodel flags and, if required, stopping autosensing
 * @param data a variant that contains a boolean indication if the mode interface is active or not
 * @param intf_obj The wan mode interface object for which the flag will be toggled on the corresponding logical netmodel interface
 * @param flag The flag that should be toggled
 * @param stop_sensing If set to true, sensing will be stopped if this mode is active
 */
static void nm_query_mode_active_handle_flags(const amxc_var_t* data, amxd_object_t* intf_obj, const char* flag, bool stop_sensing) {
    SAH_TRACEZ_IN(ME);
    bool active = GET_BOOL(data, NULL);
    const char* intf_name = object_const_string(intf_obj, "Name");

    when_null_trace(intf_obj, exit, ERROR, "Failed to get interface object");

    SAH_TRACEZ_INFO(ME, "Current mode is %s", active ? "active" : "inactive");
    if(active) {
        SAH_TRACEZ_INFO(ME, "Setting flag '%s' on interface '%s'", flag, intf_name);
        netmodel_setFlag(intf_name, flag, NULL, netmodel_traverse_this);
        if(stop_sensing) {
            mod_autosensing_stop();
        }
    } else {
        SAH_TRACEZ_INFO(ME, "Clearing flag '%s' from interface '%s'", flag, intf_name);
        netmodel_clearFlag(intf_name, flag, NULL, netmodel_traverse_this);
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

    nm_query_mode_active_handle_flags(data, intf_obj, LOGICAL4_UP_FLAG, true);
    SAH_TRACEZ_OUT(ME);
}

static void nm_query_mode6_active_cb(UNUSED const char* sig_name,
                                     const amxc_var_t* data,
                                     void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* intf_obj = (amxd_object_t*) priv;

    nm_query_mode_active_handle_flags(data, intf_obj, LOGICAL6_UP_FLAG, false);
    SAH_TRACEZ_OUT(ME);
}

static int nm_query_create_name_query(nm_query_ll_info_t* info,
                                      const char* flag) {
    SAH_TRACEZ_IN(ME);
    int rv = -2;
    amxc_string_t str_flags;
    amxc_string_init(&str_flags, 0);
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
    amxc_string_clean(&str_flags);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

int nm_query_ll_add(const char* name) {
    SAH_TRACEZ_IN(ME);
    nm_query_ll_info_t* info = NULL;
    int rv = -1;
    int index = phys_type_to_index(name);
    const char* phys_type_flag = NULL;

    when_true_trace(index < 0 || index >= physical_type_last, exit, ERROR, "'%d' is an invalid type index", index);
    info = &ll_info[index];

    when_true_status(info->used, exit, rv = 0);
    phys_type_flag = index_to_phys_type_flag(index);
    when_null_trace(phys_type_flag, exit, WARNING,
                    "Query flag for PhysicalType is not yet defined [index %d]", index);
    SAH_TRACEZ_INFO(ME, "Create query for PhysicalType '%s' [index %d]", name, index);

    info->index = index;
    rv = nm_query_create_name_query(info, phys_type_flag);
    when_failed_trace(rv, exit, ERROR, "Query for %s failed", name);
    info->used = true;

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

const char* nm_query_get_lower_layer(const char* name) {
    SAH_TRACEZ_IN(ME);
    const char* lower_layer = NULL;
    int index = phys_type_to_index(name);
    when_true_trace(index < 0 || index >= physical_type_last, exit, ERROR, "'%d' is an invalid type index", index);
    lower_layer = ll_info[index].lower_layer;
exit:
    SAH_TRACEZ_OUT(ME);
    return lower_layer;
}

static void intf_isup_queries_clean(intf_isup_queries_t** nm_queries) {
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

/**
 * @brief Create a netmodel query that will monitor if the modes is functional
 * @return Returns 0 is all queries where created successful, -1 otherwise
 */
int nm_query_mode_active(void) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    amxd_object_t* wan_mode_obj = NULL;
    wan_mode_obj = get_current_wan_mode();
    when_null_trace(wan_mode_obj, exit, ERROR, "Failed to start query, no wan mode found");

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode_obj, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        amxc_var_t data;
        intf_isup_queries_t* nm_queries = (intf_isup_queries_t*) calloc(1, sizeof(intf_isup_queries_t));

        amxc_var_init(&data);
        amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);

        when_null_trace(interface, exit_loop, ERROR, "No interface found to open query");
        when_null_trace(nm_queries, exit_loop, ERROR, "Failed to allocate memory for queries");
        amxd_object_get_params(interface, &data, amxd_dm_access_protected);

        SAH_TRACEZ_INFO(ME, "Adding queries for '%s'", interface->name);
        nm_queries->nm_ipv4_up_query = netmodel_openQuery_isUp(GET_CHAR(&data, "IPv4Reference"), "wan-manager", "ipv4-up", netmodel_traverse_this, nm_query_mode_active_cb, interface);
        nm_queries->nm_ipv6_up_query = netmodel_openQuery_isUp(GET_CHAR(&data, "IPv6Reference"), "wan-manager", "ipv6-up", netmodel_traverse_this, nm_query_mode6_active_cb, interface);
        if(interface->priv != NULL) {
            SAH_TRACEZ_ERROR(ME, "Interface already has a query, closing old query");
            intf_isup_queries_t* old_nm_queries = (intf_isup_queries_t*) interface->priv;
            intf_isup_queries_clean(&old_nm_queries);
        }
        interface->priv = nm_queries;
exit_loop:
        amxc_var_clean(&data);
    }
    rv = 0;

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

void nm_close_sensing_queries(void) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* wan_mode_obj = get_current_wan_mode();
    SAH_TRACEZ_INFO(ME, "Stopping all queries on current wan mode");
    when_null_trace(wan_mode_obj, exit, ERROR, "Failed to stop queries, no wan mode found");

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode_obj, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        const char* intf_name = object_const_string(interface, "Name");
        intf_isup_queries_t* nm_queries = (intf_isup_queries_t*) interface->priv;

        interface->priv = NULL;
        SAH_TRACEZ_INFO(ME, "Clearing queries from '%s'", interface->name);
        netmodel_clearFlag(intf_name, LOGICAL_UP_FLAGS, NULL, netmodel_traverse_this);
        intf_isup_queries_clean(&nm_queries);
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

nm_query_ll_info_t* get_nm_query_info(const char* physical_type) {
    nm_query_ll_info_t* rv = NULL;
    int index = phys_type_to_index(physical_type);

    if((index >= 0) && (index < physical_type_last)) {
        rv = &ll_info[index];
    }
    return rv;
}