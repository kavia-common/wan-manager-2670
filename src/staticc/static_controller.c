/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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
#include <amxp/amxp.h>
#include <amxc/amxc_macros.h>
#include <amxd/amxd_object.h>

#include "ctrl/mode_ctrl.h"
#include "staticc/static_controller.h"
#include "ethernet/ethernet.h"
#include "component.h"
#include "wan_manager_utils.h"
#include "dm_wan-manager.h"
#include "dhcpc/dhcpc.h"

#define ME "static-ctrl"

static amxd_status_t static_enable(const mode_ctrl_t mode,
                                   const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    ipversion_t ip_version = get_ipversion(mode);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* name = NULL;
    const char* intf_path = NULL;

    when_false(ipversion_valid(ip_version), exit);

    intf_path = get_ip_path(parameters, IPv6);
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");

    name = GET_CHAR(parameters, "Name");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    if(ip_version == IPv6) {
        rc = routing_default_ipv6_route_mod_inst(ROUTING_ORIGIN_STATIC, GETP_CHAR(parameters, "ipv6.DefaultRouter"), intf_path, true);
        when_failed_trace(rc, exit, ERROR, "Failed to set the static default IPv6 route");
    }

    if((strcmp(name, "wan") == 0)) {
        amxd_object_t* interface = amxd_dm_findf(wan_get_dm(), "%s", GET_CHAR(parameters, "intf_obj_path"));
        amxd_object_t* wan_mode_obj = amxd_object_findf(interface, "^.^.");
        rc = amxd_object_set_value(cstring_t, wan_mode_obj, ip_version == IPv6 ? "IPv6DNSMode" : "DNSMode", "Static");
        when_failed_trace(rc, exit, ERROR, "Failed to set %s to 'Static'", ip_version == IPv6 ? "IPv6DNSMode" : "DNSMode");
    }

    rc = amxd_status_ok;

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static amxd_status_t static_disable(const mode_ctrl_t mode,
                                    const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    ipversion_t ip_version = get_ipversion(mode);
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = NULL;

    when_false(ipversion_valid(ip_version), exit);

    when_true_status(ip_version == IPv4, exit, rc = amxd_status_ok); // We have nothing to do for IPv4
    intf_path = get_ip_path(parameters, IPv6);
    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");

    rc = routing_default_ipv6_route_mod_inst(ROUTING_ORIGIN_STATIC, GETP_CHAR(parameters, "ipv6.DefaultRouter"), intf_path, false);
    when_failed_trace(rc, exit, ERROR, "Failed to remove the default route");

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t static_layer(mode_ctrl_t mode,
                           amxc_var_t* const parameters,
                           bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;

    if(enable) {
        rc = static_enable(mode, parameters);
    } else {
        rc = static_disable(mode, parameters);
    }
    SAH_TRACEZ_OUT(ME);
    return rc;
}
