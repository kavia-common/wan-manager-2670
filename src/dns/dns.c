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

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb_types.h>
#include <amxo/amxo.h>

#include "ctrl/mode_ctrl.h"
#include "wan_manager_utils.h"
#include "component.h"
#include "dns/dns.h"
#include "dm_wan-manager.h"

#define ME "wan-man"
#define WANMANAGER_FMT "wanmanager-"

static uint32_t dns_relay_forwarding_index = 0;

static char* get_logical_iface_path(amxd_object_t* interface) {
    SAH_TRACEZ_IN(ME);
    const char* name = NULL;
    char* logical_intf_named = NULL;
    char* logical_intf = NULL;

    name = object_const_string(interface, "Name");
    when_str_empty_trace(name, exit, ERROR, "Failed to get name of interface %s", amxd_object_get_name(interface, AMXD_OBJECT_INDEXED));
    logical_intf_named = create_logical_path(name);
    logical_intf = component_get_path_instance(logical_get_context(), logical_intf_named);
    when_str_empty_trace(logical_intf, exit, ERROR, "Failed to get Logical interface belonging to %s", name);

exit:
    free(logical_intf_named);
    SAH_TRACEZ_OUT(ME);
    return logical_intf;
}

static char* get_config_path(void) {
    char* config_path = NULL;
    amxc_string_t search_path;
    const char* prefix = NULL;

    amxc_string_init(&search_path, 0);

    prefix = GET_CHAR(amxo_parser_get_config(wan_get_parser(), "vendor_prefix"), "dns");
    if(prefix == NULL) {
        prefix = "";
    }

    amxc_string_setf(&search_path, "Device.DNS.Relay.%sConfig.*.", prefix); // terminate with . to only return instances and not the template

    config_path = component_get_path_instance(dns_get_context(), amxc_string_get(&search_path, 0));

    amxc_string_clean(&search_path);
    return config_path;
}

static amxd_status_t remove_static_dnsservers(void) {
    amxc_string_t alias;
    amxd_status_t rc = amxd_status_unknown_error;
    amxb_bus_ctx_t* bus_ctx = NULL;

    amxc_string_init(&alias, 0);

    when_false_status(dns_relay_forwarding_index > 0, exit, rc = amxd_status_ok);

    bus_ctx = dns_get_context();

    while(dns_relay_forwarding_index > 0) {
        amxc_var_t values;
        amxc_var_t ret;

        amxc_string_setf(&alias, WANMANAGER_FMT "%u", dns_relay_forwarding_index);

        amxc_var_init(&values);
        amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, &values, "Alias", amxc_string_get(&alias, 0));
        amxc_var_init(&ret);

        rc = amxb_call(bus_ctx, "DNS.", "DeleteForwarding", &values, &ret, 5);

        amxc_var_clean(&values);
        amxc_var_clean(&ret);

        when_failed_trace(rc, exit, INFO, "Failed to remove DNS.Relay.Forwarding.%s.: %d", amxc_string_get(&alias, 0), rc); // don't continue, otherwise those instances will never be removed

        dns_relay_forwarding_index--;
    }

exit:
    if(dns_relay_forwarding_index > 0) {
        SAH_TRACEZ_WARNING(ME, "Not all static dnsservers have been removed!");
    }
    amxc_string_clean(&alias);
    return rc;
}

static amxd_status_t set_forwarding(amxd_object_t* address_obj, amxb_bus_ctx_t* bus_ctx, const char* iface) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t alias;
    amxc_var_t list;

    amxc_var_init(&list);
    amxc_string_init(&alias, 0);

    amxc_var_convert(&list, amxd_object_get_param_value(address_obj, "DNSServers"), AMXC_VAR_ID_LIST);

    when_null_status(amxc_var_get_first(&list), exit, rc = amxd_status_ok);

    amxc_var_for_each(dnsserver, &list) {
        amxc_var_t values;
        amxc_var_t ret;
        uint32_t next_index = 0;

        next_index = dns_relay_forwarding_index + 1;

        amxc_string_setf(&alias, WANMANAGER_FMT "%u", next_index);

        amxc_var_init(&values);
        amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(bool, &values, "Enable", true);
        amxc_var_add_key(cstring_t, &values, "DNSServer", GET_CHAR(dnsserver, 0));
        amxc_var_add_key(cstring_t, &values, "Interface", iface);
        amxc_var_add_key(cstring_t, &values, "Type", "Static");
        amxc_var_add_key(cstring_t, &values, "Alias", amxc_string_get(&alias, 0));
        amxc_var_add_key(bool, &values, "AddInstance", true);
        amxc_var_init(&ret);

        rc = amxb_call(bus_ctx, "DNS.", "SetForwarding", &values, &ret, 5);

        amxc_var_clean(&values);
        amxc_var_clean(&ret);

        when_failed_trace(rc, exit, ERROR, "Failed to set DNS.Relay.Forwarding.%s.: %d", amxc_string_get(&alias, 0), rc);

        dns_relay_forwarding_index = next_index;
    }

exit:
    amxc_var_clean(&list);
    amxc_string_clean(&alias);
    return rc;
}

static amxd_status_t add_static_dnsservers(amxd_object_t* iface_obj) {
    char* iface = NULL;
    amxb_bus_ctx_t* bus_ctx = NULL;
    amxd_status_t rc = amxd_status_ok;

    bus_ctx = dns_get_context();

    if(dns_relay_forwarding_index > 0) {
        SAH_TRACEZ_WARNING(ME, "Not all static dnsservers were cleaned up last time!");
        remove_static_dnsservers(); // best effort, ignores fails
    }

    iface = get_logical_iface_path(iface_obj);
    when_str_empty(iface, exit);

    amxd_object_for_each(instance, it, amxd_object_findf(iface_obj, "IPv4Address")) {
        amxd_object_t* address_obj = amxc_container_of(it, amxd_object_t, it);
        rc = set_forwarding(address_obj, bus_ctx, iface);
        when_failed(rc, exit); // don't continue
    }

    amxd_object_for_each(instance, it, amxd_object_findf(iface_obj, "IPv6Address")) {
        amxd_object_t* address_obj = amxc_container_of(it, amxd_object_t, it);
        rc = set_forwarding(address_obj, bus_ctx, iface);
        when_failed(rc, exit); // don't continue
    }

exit:
    free(iface);
    return rc;
}

static amxd_status_t set_dnsmode(amxd_object_t* wan_mode) {
    amxc_var_t values;
    amxc_var_t ret;
    char* config_path = NULL;
    const char* dnsmode = NULL;
    amxd_status_t rc = amxd_status_unknown_error;
    uint32_t count = 0;

    amxc_var_init(&values);
    amxc_var_init(&ret);

    amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);

    dnsmode = object_const_string(wan_mode, "DNSMode");
    if(!str_empty(dnsmode)) {
        amxc_var_add_key(cstring_t, &values, "DNSMode", dnsmode);
        count++;
    }

    dnsmode = object_const_string(wan_mode, "IPv6DNSMode");
    if(!str_empty(dnsmode)) {
        amxc_var_add_key(cstring_t, &values, "IPv6DNSMode", dnsmode);
        count++;
    }

    if(count == 0) {
        rc = amxd_status_ok;
        SAH_TRACEZ_INFO(ME, "Skip set DNSMode");
        goto exit;
    }

    // at 31 Jan 2024 the dns plugin only uses the first DNS.Relay.Config instance for [IPv6]DNSMode
    config_path = get_config_path();
    when_str_empty_trace(config_path, exit, ERROR, "Failed to set [IPv6]DNSMode");
    SAH_TRACEZ_INFO(ME, "Set [IPv6]DNSMode of '%s'", config_path);

    rc = amxb_set(dns_get_context(), config_path, &values, &ret, 5);
    when_failed_trace(rc, exit, ERROR, "Failed to set [IPv6]DNSMode: %d", rc);

exit:
    amxc_var_clean(&values);
    amxc_var_clean(&ret);
    free(config_path);
    return rc;
}

amxd_status_t dns_mode_set(amxd_object_t* wan_mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    SAH_TRACEZ_IN(ME);

    when_null_trace(dns_get_context(), exit, ERROR, "Bus ctx for DNS. not found");

    rc = set_dnsmode(wan_mode);
    when_failed(rc, exit);

    amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
        amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
        rc = add_static_dnsservers(interface);
        when_failed(rc, exit);
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dns_mode_unset(void) {
    amxd_status_t rc = amxd_status_unknown_error;
    SAH_TRACEZ_IN(ME);

    when_null_trace(dns_get_context(), exit, ERROR, "Bus ctx for DNS. not found");

    rc = remove_static_dnsservers();

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}
