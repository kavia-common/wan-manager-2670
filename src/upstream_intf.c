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
#include <stdbool.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxm/amxm.h>

#include "netmodel/nm_query.h"

#include "component.h"
#include "wan_manager_utils.h"
#include "netmodel/nm_query.h"
#include "upstream_intf.h"

#define ME "wan-man"

typedef int (* upstream_toggle_func_t)(nm_query_ll_info_t*, bool);
static int toggle_ethernet(nm_query_ll_info_t* info, bool enable);
static int toggle_xpon(nm_query_ll_info_t* info, bool enable);

/**
 * phys_types are the names used in the datamodel:
 * ${prefix_}WANManager.WAN.{i}.PhysicalType
 * phys_types_flags are the names of the flags used in the netmodel query
 * NULL stands for 'not yet defined'
 *
 * Make sure their order of appearance matches
 */
const char* phys_types[physical_type_last] = {
    "Ethernet", "Bridge", "ADSL", "VDSL", "SFP", "GPON", "GFAST", "WWAN"
};
const char* phys_types_flags[physical_type_last] = {
    "eth_intf", "bridge", NULL, NULL, NULL, "xpon", NULL, NULL
};

static const upstream_toggle_func_t upstream_toggle[physical_type_last] = {
    toggle_ethernet, NULL, NULL, NULL, NULL, toggle_xpon, NULL, NULL
};

/**
 * This function will be called from toggle_upstream_intf when upstream_toggle for the given index is NULL.
 * If this function is called, a function should be implemented to handle the type for which it is called.
 */
static int toggle_dummy(nm_query_ll_info_t* info, UNUSED bool enable) {
    SAH_TRACEZ_IN(ME);
    when_null_trace(info, exit, ERROR, "No info structure provided");
    SAH_TRACEZ_ERROR(ME, "No toggle function defined for %s", index_to_phys_type(info->index));
exit:
    SAH_TRACEZ_OUT(ME);
    return -1;
}

/**
 * This function is intended to toggle upstream interfaces of type "Ethernet".
 * The expected upstream_intf_path is something like "Device.Ethernet.Interface.1.".
 * The enable parameter for this instance is toggled
 */
static int toggle_ethernet(nm_query_ll_info_t* info, bool enable) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    when_null_trace(info, exit, ERROR, "No info structure provided");
    SAH_TRACEZ_INFO(ME, "Toggling ethernet to %d", enable);

    rv = component_set_enable(info->upstream_intf_path, ethernet_get_context(), enable);

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * This function is intended to toggle upstream interfaces of type "GPON".
 * The expected upstream_intf_path is something like "Device.XPON.ONU.1.EthernetUNI.1.".
 * Since we want to toggle the ONU instead of the EthernetUNI, everything starting from "EthernetUNI" is removed from the path.
 * This would result in a path like "Device.XPON.ONU.1." for which the enable parameter is toggled
 */
static int toggle_xpon(nm_query_ll_info_t* info, bool enable) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    int pos = -1;
    amxc_string_t path;

    amxc_string_init(&path, 0);

    when_null_trace(info, exit, ERROR, "No info structure provided");
    SAH_TRACEZ_INFO(ME, "Toggling xpon to %d", enable);

    amxc_string_setf(&path, "%s", info->upstream_intf_path);
    pos = amxc_string_search(&path, "EthernetUNI", 0);
    amxc_string_remove_at(&path, pos, UINT32_MAX);
    rv = component_set_enable(amxc_string_get(&path, 0), xpon_get_context(), enable);

exit:
    amxc_string_clean(&path);
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Converts the physical type from a string to the corresponding index.
 *  This index can be used to find the flags or toggle function corresponding with this type.
 * @param phys_type The physical type in string as it is stored in the "PhysicalType" parameter
 * @return A value between 0 and physical_type_last if a valid physical type was provided, -1 otherwise.
   error code and no changes in the data model are done.
 */
int phys_type_to_index(const char* phys_type) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    int cnt = 0;
    when_str_empty_trace(phys_type, exit, WARNING, "PhysicalType name is empty");
    while(cnt < (int) physical_type_last) {
        if(strcmp(phys_type, phys_types[cnt]) == 0) {
            rv = cnt;
            break;
        }
        cnt++;
    }
exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Converts the index to a physical type string
 * @param index A value between 0 and physical_type_last that corresponds with the wanted physical type
 * @return The physical type in string as it is stored in the "PhysicalType" parameter, NULL if no valid index was provided
 */
const char* index_to_phys_type(const int index) {
    SAH_TRACEZ_IN(ME);
    const char* rv = NULL;
    if((index >= 0) && (index < physical_type_last)) {
        rv = phys_types[index];
    }
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Converts the index to a physical type flag string
 * @param index A value between 0 and physical_type_last that corresponds with the wanted physical type for which the flags are wanted
 * @return The flags for the corresponding physical type in string, NULL if no valid index was provided
 */
const char* index_to_phys_type_flag(const int index) {
    SAH_TRACEZ_IN(ME);
    const char* rv = NULL;
    if((index >= 0) && (index < physical_type_last)) {
        rv = phys_types_flags[index];
    }
    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * @brief Will call the upstream_toggle function for the requested physical_type
 * @param physical_type The physical type in string as it is stored in the "PhysicalType" parameter
 * @return 0 if successful, -1 if an error occurred
 */
int toggle_upstream_intf(const char* physical_type, bool enable) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;
    nm_query_ll_info_t* info = NULL;

    info = get_nm_query_info(physical_type);
    when_null_trace(info, exit, ERROR, "No info structure found for physical type '%s'", physical_type);

    if(upstream_toggle[info->index] != NULL) {
        rv = upstream_toggle[info->index](info, enable);
    } else {
        rv = toggle_dummy(info, enable);
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}
