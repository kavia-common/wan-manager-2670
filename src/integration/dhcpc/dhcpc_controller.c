/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2021 SoftAtHome
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

#include "ctrl/mode_ctrl.h"
#include "integration/dhcpc/dhcpc.h"
#include "integration/ethernet/ethernet.h"
#include "component.h"

#define ME "dhcpc-ctrl"

static amxb_bus_ctx_t* dhcpv4_ctx = NULL;
static amxb_bus_ctx_t* dhcpv6_ctx = NULL;
static amxb_bus_ctx_t* ip_ctx = NULL;
static amxb_bus_ctx_t* ip_get_context(void);
static amxb_bus_ctx_t* dhcpv4_get_context(void);
static amxb_bus_ctx_t* dhcpv6_get_context(void);

static char* dhcpc_add_client_instance(amxb_bus_ctx_t* ctx,
                                       char ip,
                                       const char* name,
                                       const char* lower_layer);

static char* dhcpc_get_client(bool ipv4,
                              const char* intf_path,
                              const char* intf_alias) {
    amxc_string_t query;
    amxb_bus_ctx_t* ctx = ipv4 ? dhcpv4_get_context() : dhcpv6_get_context();
    char ip = ipv4 ? '4' : '6';
    char* path = NULL;
    amxc_string_init(&query, 0);
    amxc_string_setf(&query, "DHCPv%c.Client.[Interface=='%s'].", ip, intf_path);
    path = component_get_path_instance(ctx, amxc_string_get(&query, 0));
    if((NULL == path) && (NULL != intf_alias)) {
        SAH_TRACEZ_INFO(ME, "DHCPv%c.Client.[Interface=='%s']. instance does not exist." \
                        " Create it", ip, intf_path);
        path = dhcpc_add_client_instance(ctx, ip, intf_alias, intf_path);
        when_null_trace(path, exit, ERROR, "Add DHCPv%c Client instance error", ip);
    }
exit:
    amxc_string_clean(&query);
    return path;
}

amxd_status_t dhcpc_enable(mode_ctrl_t mode,
                           const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    char* dhcpv4_path = NULL;
    char* dhcpv6_path = NULL;
    const char* intf_alias = NULL;
    const char* intf_path = NULL;
    const char* lower_layer = NULL;

    when_null(parameters, exit);
    lower_layer = GETP_CHAR(parameters, "LowerLayer");

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Enable VLAN interface");
        ethernet_vlan_set_enable(parameters, true);
    }
    intf_alias = GETP_CHAR(parameters, "Alias");
    intf_path = GETP_CHAR(parameters, "IPReference");

    when_str_empty(intf_alias, exit);
    when_str_empty(intf_path, exit);
    if((mode & IPv4_DHCP) != 0) {
        dhcpv4_path = dhcpc_get_client(true, intf_path, intf_alias);
        when_str_empty(dhcpv4_path, exit);
    }
    if((mode & IPv6_DHCP) != 0) {
        dhcpv6_path = dhcpc_get_client(false, intf_path, intf_alias);
        when_str_empty(dhcpv6_path, exit);
    }
    if((mode & TYPE_VLAN) != 0) {
        lower_layer = GETP_CHAR(parameters, "VLANTermination");
    }
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", lower_layer);
    when_failed(rc, exit);

    if((mode & IPv4_DHCP) != 0) {
        rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), true);
        when_failed_trace(rc, exit, ERROR, "DHCPv4 Enable failed");
    }
    if((mode & IPv6_DHCP) != 0) {
        rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), true);
        when_failed_trace(rc, exit, ERROR, "DHCPv6 Enable failed");
    }
exit:
    free(dhcpv4_path);
    free(dhcpv6_path);
    return rc;
}

amxd_status_t dhcpc_disable(mode_ctrl_t mode,
                            const amxc_var_t* const parameters) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = NULL;
    char* dhcpv4_path = NULL;
    char* dhcpv6_path = NULL;

    when_null(parameters, exit);
    intf_path = GETP_CHAR(parameters, "IPReference");
    when_str_empty(intf_path, exit);
    rc = component_set_str_param(intf_path, ip_get_context(), "LowerLayers", "");
    when_failed(rc, exit);

    if((mode & TYPE_VLAN) != 0) {
        SAH_TRACEZ_INFO(ME, "Disable VLAN interface");
        ethernet_vlan_set_enable(parameters, false);
    }

    if((mode & IPv4_DHCP) != 0) {
        dhcpv4_path = dhcpc_get_client(true, intf_path, NULL);
        if(dhcpv4_path != NULL) {
            SAH_TRACEZ_INFO(ME, "DHCPv4 path for %s -> %s", intf_path, dhcpv4_path);
            rc = component_set_enable(dhcpv4_path, dhcpv4_get_context(), false);
            when_failed(rc, exit);
        } else {
            SAH_TRACEZ_INFO(ME, "No DHCPv4 client found with Interface='%s'", intf_path);
            rc = amxd_status_ok;
        }
    }
    if((mode & IPv6_DHCP) != 0) {
        dhcpv6_path = dhcpc_get_client(false, intf_path, NULL);
        if(dhcpv6_path != NULL) {
            SAH_TRACEZ_INFO(ME, "DHCPv6 path for %s -> %s", intf_path, dhcpv6_path);
            rc = component_set_enable(dhcpv6_path, dhcpv6_get_context(), false);
            when_failed(rc, exit);
        } else {
            SAH_TRACEZ_INFO(ME, "No DHCPv6 client found with Interface='%s'", intf_path);
            rc = amxd_status_ok;
        }
    }
exit:
    free(dhcpv4_path);
    free(dhcpv6_path);
    return rc;
}

static amxb_bus_ctx_t* ip_get_context(void) {
    if(NULL == ip_ctx) {
        ip_ctx = amxb_be_who_has("IP.");
    }
    return ip_ctx;
}

static amxb_bus_ctx_t* dhcpv4_get_context(void) {
    if(NULL == dhcpv4_ctx) {
        dhcpv4_ctx = amxb_be_who_has("DHCPv4.");
    }
    return dhcpv4_ctx;
}

static amxb_bus_ctx_t* dhcpv6_get_context(void) {
    if(NULL == dhcpv6_ctx) {
        dhcpv6_ctx = amxb_be_who_has("DHCPv6.");
    }
    return dhcpv6_ctx;
}

static char* dhcpc_add_client_instance(amxb_bus_ctx_t* ctx,
                                       char ip,
                                       const char* name,
                                       const char* lower_layer) {
    char* path = NULL;
    amxc_var_t parameters;
    amxc_string_t alias;
    amxc_string_t instance;

    amxc_var_init(&parameters);
    amxc_string_init(&instance, 0);
    amxc_string_init(&alias, 0);
    when_str_empty(name, exit);
    when_str_empty(lower_layer, exit);
    amxc_string_setf(&alias, "wanm-%s", name);
    amxc_string_setf(&instance, "DHCPv%c.Client.", ip);

    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "Alias", amxc_string_get(&alias, 0));
    amxc_var_add_key(cstring_t, &parameters, "Interface", lower_layer);
    amxc_var_add_key(bool, &parameters, "Enable", false);

    path = component_add_instance(amxc_string_get(&instance, 0), &parameters, ctx);
    SAH_TRACEZ_INFO(ME, "Create instance '%s' -> '%s'", amxc_string_get(&instance, 0), path);
exit:
    amxc_var_clean(&parameters);
    amxc_string_clean(&alias);
    amxc_string_clean(&instance);
    return path;
}

