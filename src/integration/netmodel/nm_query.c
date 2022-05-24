/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
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

#include "integration/netmodel/nm_query.h"

#define ME "netmod-ctrl"
/**
 * phys_types are the names used in the datamodel:
 * ${prefix_}WANManager.WAN.{i}.PhysicalType
 * phys_types_flags are the names of the flags used in the netmodel query
 * NULL stands got 'not yet defined'
 *
 * Make sure their order of appearance match
 */
const char* phys_types[physical_type_last] = {
    "Ethernet", "Bridge", "ADSL", "VDSL", "SFP", "GPON", "GFAST", "WWAN"
};
const char* phys_types_flags[physical_type_last] = {
    "eth_intf", "bridge", NULL, NULL, NULL, NULL, NULL, NULL
};
nm_query_ll_info_t ll_info[physical_type_last];

void nm_query_ll_init(void) {
    memset((void*) ll_info, 0, sizeof(ll_info));
}

void nm_query_ll_cleanup(void) {
    nm_query_ll_info_t* info = ll_info;
    for(int cnt = 0; cnt < physical_type_last; cnt++, info++) {
        if(info->used) {
            netmodel_closeQuery(info->q_name);
            netmodel_closeQuery(info->q_intf_path);
            free(info->intf_name);
            free(info->lower_layer);
        }
    }
}

static int nm_query_ll_name_to_index(const char* name) {
    int rv = -1;
    int cnt = 0;
    when_str_empty_trace(name, exit, WARNING, "PhysicalType name is empty");
    while(cnt < (int) physical_type_last) {
        if(strcmp(name, phys_types[cnt]) == 0) {
            rv = cnt;
            break;
        }
        cnt++;
    }
exit:
    return rv;
}

static void nm_query_response_ll_cb(UNUSED const char* sig_name,
                                    const amxc_var_t* data,
                                    void* priv) {
    nm_query_ll_info_t* info = (nm_query_ll_info_t*) priv;
    const char* lower_layer = NULL;
    when_null_trace(info, exit, ERROR, "private data is null");
    when_true(((info->index < 0) || (info->index >= physical_type_last)), exit);
    lower_layer = amxc_var_constcast(cstring_t, data);
    SAH_TRACEZ_INFO(ME, "LowerLayer query for PhysicalType = %s -> %s",
                    phys_types[info->index], lower_layer);
    when_str_empty(lower_layer, exit);
    free(info->lower_layer);
    info->lower_layer = strdup(lower_layer);
exit:
    return;
}

static void nm_query_response_name_cb(UNUSED const char* sig_name,
                                      const amxc_var_t* data,
                                      void* priv) {
    amxc_string_t intf_path;
    nm_query_ll_info_t* info = (nm_query_ll_info_t*) priv;
    const char* name = NULL;
    amxc_string_init(&intf_path, 0);
    when_null_trace(info, exit, ERROR, "private data is null");
    when_true(((info->index < 0) || (info->index >= physical_type_last)), exit);
    name = amxc_var_constcast(cstring_t, amxc_var_get_first(data));
    SAH_TRACEZ_INFO(ME, "Name query for PhysicalType = %s -> %s",
                    phys_types[info->index], name);
    when_str_empty(name, exit);
    if(info->intf_name != NULL) {
        when_true((strcmp(info->intf_name, name) == 0), exit);
        netmodel_closeQuery(info->q_intf_path);
        free(info->intf_name);
    }
    info->intf_name = strdup(name);

    // create another query to get the LowerLayer
    amxc_string_setf(&intf_path, "NetModel.Intf.%s.", name);
    info->q_intf_path = netmodel_openQuery_getFirstParameter(amxc_string_get(&intf_path, 0),
                                                             "wan-manager", "InterfacePath", "",
                                                             netmodel_traverse_one_level_up,
                                                             nm_query_response_ll_cb, priv);
exit:
    amxc_string_clean(&intf_path);
    return;
}

static int nm_query_create_name_query(nm_query_ll_info_t* info,
                                      const char* flag) {
    int rv = -2;
    amxc_string_t str_flags;
    amxc_string_init(&str_flags, 0);
    amxc_string_setf(&str_flags, "%s && upstream", flag);
    info->q_name = netmodel_openQuery_getIntfs("NetModel.Intf.resolver.", "wan-manager",
                                               amxc_string_get(&str_flags, 0),
                                               netmodel_traverse_all,
                                               nm_query_response_name_cb, (void*) info);
    if(info->q_name != NULL) {
        rv = 0;
    }
    amxc_string_clean(&str_flags);
    return rv;
}

int nm_query_ll_add(const char* name) {
    nm_query_ll_info_t* info = NULL;
    int rv = -1;
    int index = nm_query_ll_name_to_index(name);
    when_true(((index < 0) || (index >= physical_type_last)), exit);
    info = &ll_info[index];
    when_true_status(info->used, exit, rv = 0);
    when_null_trace(phys_types_flags[index], exit, WARNING,
                    "Query flag for PhysicalType is not yet defined");
    rv = nm_query_create_name_query(info, phys_types_flags[index]);
    when_failed_trace(rv, exit, ERROR, "Query for %s failed", name);
    // index is used only for debug information
    info->index = index;
    info->used = true;
exit:
    return rv;
}

const char* nm_query_get_lower_layer(const char* name) {
    const char* lower_layer = NULL;
    int index = nm_query_ll_name_to_index(name);
    when_true(index < 0, exit);
    lower_layer = ll_info[index].lower_layer;
exit:
    return lower_layer;
}