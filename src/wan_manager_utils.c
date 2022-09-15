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
#include "component.h"

#include "wan_manager_utils.h"
#define ME "wan-man"

;
static amxb_bus_ctx_t* ip_ctx = NULL;
static amxb_bus_ctx_t* dhcpv4_ctx = NULL;
static amxb_bus_ctx_t* dhcpv6_ctx = NULL;
static amxb_bus_ctx_t* ppp_ctx = NULL;
static amxb_bus_ctx_t* routing_ctx = NULL;

amxb_bus_ctx_t* ip_get_context(void) {
    if(NULL == ip_ctx) {
        ip_ctx = amxb_be_who_has("IP.");
    }
    return ip_ctx;
}

amxb_bus_ctx_t* dhcpv4_get_context(void) {
    if(NULL == dhcpv4_ctx) {
        dhcpv4_ctx = amxb_be_who_has("DHCPv4.");
    }
    return dhcpv4_ctx;
}

amxb_bus_ctx_t* dhcpv6_get_context(void) {
    if(NULL == dhcpv6_ctx) {
        dhcpv6_ctx = amxb_be_who_has("DHCPv6.");
    }
    return dhcpv6_ctx;
}

amxb_bus_ctx_t* ppp_get_context(void) {
    if(NULL == ppp_ctx) {
        ppp_ctx = amxb_be_who_has("PPP.");
    }
    return ppp_ctx;
}

amxb_bus_ctx_t* routing_get_context(void) {
    if(NULL == routing_ctx) {
        routing_ctx = amxb_be_who_has("Routing.");
    }
    return routing_ctx;
}

amxd_status_t ip_addr_toggle(const char* intf_path, const char* addr_type, bool enable) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t addr_path;
    amxc_string_init(&addr_path, 0);

    amxc_string_setf(&addr_path, "%sIPv4Address.[AddressingType == '%s'].", intf_path, addr_type);
    rc = component_set_enable(amxc_string_get(&addr_path, 0), ip_get_context(), enable);
    when_failed_trace(rc, exit, ERROR, "Failed to %s ip address instance '%s'", enable ? "enable" : "disable", amxc_string_get(&addr_path, 0));

exit:
    amxc_string_clean(&addr_path);
    return rc;
}

amxd_status_t routing_default_route_set_origin(const char* ip_path, const char* routing_origin) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t route_path;
    amxc_string_init(&route_path, 0);

    amxc_string_setf(&route_path, "Device.Routing.Router.1.IPv4Forwarding.[Interface=='%s' && DestIPAddress=='0.0.0.0' && DestSubnetMask=='0.0.0.0'].", ip_path);
    rc = component_set_str_param(amxc_string_get(&route_path, 0), routing_get_context(), "Origin", routing_origin);
    when_failed_trace(rc, exit, ERROR, "Failed to set default route '%s' origin to '%s'", amxc_string_get(&route_path, 0), routing_origin);

exit:
    amxc_string_clean(&route_path);
    return rc;
}