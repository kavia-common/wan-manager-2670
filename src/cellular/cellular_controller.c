/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice,
** this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** Subject to the terms and conditions of this license, each copyright holder
** and contributor hereby grants to those receiving rights under this license
** a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable
** (except for failure to satisfy the conditions of this license) patent license
** to make, have made, use, offer to sell, sell, import, and otherwise transfer
** this software, where such license applies only to those patent claims, already
** acquired or hereafter acquired, licensable by such copyright holder or contributor
** that are necessarily infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright holders and
** non-copyrightable additions of contributors, in source or binary form) alone;
** or
**
** (b) combination of their Contribution(s) with the work of authorship to which
** such Contribution(s) was added by such copyright holder or contributor, if,
** at the time the Contribution is added, such addition causes such combination
** to be necessarily infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any copyright
** holder or contributor is granted under this license, whether expressly, by
** implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
** USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include <string.h>
#include <stdlib.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxc/amxc_macros.h>

#include "ctrl/mode_ctrl.h"
#include "cellular/cellular.h"
#include "component.h"
#include "dm_wan_mode.h"
#include "dm_wan-manager.h"

#define ME "cellular-ctrl"

#define CELLULAR_IPv4_TYPE "ipv4"
#define CELLULAR_IPv6_TYPE "ipv6"
#define CELLULAR_BOTH_TYPE "ipv4v6"

static const char* get_cellular_ip_type(amxc_var_t* const parameters) {
    const char* ip_type = NULL;
    bool cellular_ipv4 = (wan_mode_convert_from_str(GET_CHAR(parameters, "IPv4Mode"), IPv4) == IPv4_CELLULAR);
    bool cellular_ipv6 = (wan_mode_convert_from_str(GET_CHAR(parameters, "IPv6Mode"), IPv6) == IPv6_CELLULAR);

    if(cellular_ipv4 && cellular_ipv6) {
        ip_type = CELLULAR_BOTH_TYPE;
    } else if(cellular_ipv4) {
        ip_type = CELLULAR_IPv4_TYPE;
    } else if(cellular_ipv6) {
        ip_type = CELLULAR_IPv6_TYPE;
    }

    return ip_type;
}

static char* get_cellular_accesspoint_search_path(const char* cellular_interface) {
    char* path = NULL;
    amxc_string_t search_path;

    amxc_string_init(&search_path, 0);

    when_str_empty_trace(cellular_interface, exit, ERROR, "Empty cellular interface");
    amxc_string_setf(&search_path, "Cellular.AccessPoint.[Interface=='%s']", cellular_interface);

    path = amxc_string_take_buffer(&search_path);

exit:
    amxc_string_clean(&search_path);
    return path;
}

static amxd_status_t cellular_enable(UNUSED mode_ctrl_t mode,
                                     amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* cellular_path = NULL;
    const char* current_iptype = NULL;
    const char* new_iptype = get_cellular_ip_type(parameters);
    const char* wan_mode_str = GET_CHAR(parameters, "wanmode_name");
    char* accesspoint_path = NULL;
    char* iptype_parameter = get_prefixed_parameter_name("IPType");
    amxd_object_t* wan_mode = get_wan_mode(wan_mode_str);
    amxc_var_t* ip_type_param;

    amxc_var_new(&ip_type_param);

    when_str_empty_trace(new_iptype, exit, ERROR, "Failed to get new %s", iptype_parameter);
    when_null_trace(wan_mode, exit, ERROR, "Failed to get WANMode");
    cellular_path = object_const_string(wan_mode, "PhysicalReference");
    when_str_empty_trace(cellular_path, exit, ERROR, "Failed to get Cellular interface path");
    accesspoint_path = get_cellular_accesspoint_search_path(cellular_path);
    when_null_trace(accesspoint_path, exit, ERROR, "Could not get Cellular Accesspoint path");

    component_get_param(ip_type_param, accesspoint_path, cellular_get_context(), iptype_parameter);

    current_iptype = GETP_CHAR(GETP_ARG(ip_type_param, "0.0"), iptype_parameter);
    when_str_empty_trace(current_iptype, exit, ERROR, "Failed to get current %s", iptype_parameter);

    if(strcmp(current_iptype, new_iptype) != 0) {
        rc = component_set_str_param(accesspoint_path, cellular_get_context(), iptype_parameter, new_iptype);
        when_failed_trace(rc, exit, ERROR, "Failed to set '%s' to '%s'", iptype_parameter, new_iptype);
    }

    rc = component_set_enable(accesspoint_path, cellular_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Failed to enable %s", accesspoint_path);

exit:
    amxc_var_delete(&ip_type_param);
    free(iptype_parameter);
    free(accesspoint_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t cellular_disable(UNUSED mode_ctrl_t mode,
                                      amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* cellular_path = NULL;
    const char* wan_mode_str = GET_CHAR(parameters, "wanmode_name");
    amxd_object_t* wan_mode = get_wan_mode(wan_mode_str);
    const char* accesspoint_path = NULL;
    bool is_slice = !str_empty(intf_type) && strcmp(intf_type, "slice") == 0;

    when_null_trace(wan_mode, exit, ERROR, "Failed to get WANMode");
    cellular_path = object_const_string(wan_mode, "PhysicalReference");
    when_str_empty_trace(cellular_path, exit, ERROR, "Failed to get Cellular interface path");

    if(is_slice) {
        // AccessPoint must be defined in DM for slices
        accesspoint_path = strdup(get_cellular_accesspoint_path(parameters));
    } else {
        // For regular Cellular connections search the AccessPoint based on Physical Reference
        accesspoint_path = find_cellular_accesspoint_path(cellular_path);
    }

    rc = component_set_enable(accesspoint_path, cellular_get_context(), false);
    when_failed_trace(rc, exit, ERROR, "Failed to disable %s", accesspoint_path);

exit:
    free(accesspoint_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t cellular_layer(mode_ctrl_t mode,
                             amxc_var_t* const parameters,
                             bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    if(enable) {
        rc = cellular_enable(mode, parameters);
    } else {
        rc = cellular_disable(mode, parameters);
    }

    return rc;
}
