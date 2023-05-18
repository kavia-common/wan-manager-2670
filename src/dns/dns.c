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

#include "ctrl/mode_ctrl.h"
#include "wan_manager_utils.h"
#include "component.h"
#include "dns/dns.h"

#define ME "wan-man"

typedef enum {
    DNS_DHCPv4              = 0b00010,
    DNS_DHCPv6              = 0b00100,
    DNS_RouterAdvertisement = 0b01000,
    DNS_IPCP                = 0b10000,
    DNS_STATIC              = 0b00001,
    DNS_DYNAMIC             = 0b11110,
    DNS_NONE                = 0b11111
} dns_mode_t;

typedef struct {
    dns_mode_t dns_mode;
    const char* dns_str;
} dns_mode_cnv_t;

static dns_mode_cnv_t dns_mode_cnv[] = {
    {DNS_STATIC, "Static"},
    {DNS_DHCPv4, "DHCPv4"},
    {DNS_DHCPv6, "DHCPv6"},
    {DNS_DYNAMIC, "Dynamic"},
    {DNS_IPCP, "IPCP"},
    {DNS_RouterAdvertisement, "RouterAdvertisement"},
    {DNS_NONE, NULL}
};

static dns_mode_t string_to_dns_mode(const char* dns_mode) {
    SAH_TRACEZ_IN(ME);
    dns_mode_cnv_t* lookup = dns_mode_cnv;
    dns_mode_t dns = DNS_NONE;

    while(lookup->dns_str != NULL) {
        if(0 == strcmp(dns_mode, lookup->dns_str)) {
            dns = lookup->dns_mode;
            break;
        }
        lookup++;
    }
    SAH_TRACEZ_OUT(ME);
    return dns;
}

static void dns_server_build_args(amxc_var_t* dns_servers, amxd_object_t* interface, bool ipv4) {
    SAH_TRACEZ_IN(ME);
    amxc_llist_it_t* ip_it = NULL;
    amxc_llist_t* ip_var_list = NULL;
    amxc_var_t ip_dns_servers_var;
    amxc_llist_t* ip_dns_servers_list = NULL;
    amxc_var_t* ip_var = NULL;
    amxd_object_t* ip_addr = NULL;
    const char* dns_server_ip = NULL;
    const char* ip_ref_param = ipv4 ? "IPv4Reference" : "IPv6Reference";
    const char* ip_addr_param = ipv4 ? ".IPv4Address." : ".IPv6Address.";
    char* ip_ref = NULL;

    ip_ref = amxd_object_get_value(cstring_t, interface, ip_ref_param, NULL);

    amxc_var_init(&ip_dns_servers_var);

    ip_it = amxd_object_first_instance(amxd_object_findf(interface, "%s", ip_addr_param));
    ip_addr = amxc_container_of(ip_it, amxd_object_t, it);

    dns_server_ip = GET_CHAR(amxd_object_get_param_value(ip_addr, "DNSServers"), NULL);
    amxc_var_set(cstring_t, &ip_dns_servers_var, dns_server_ip);
    ip_dns_servers_list = amxc_var_dyncast(amxc_llist_t, &ip_dns_servers_var);

    ip_var = amxc_var_add_key(amxc_llist_t, dns_servers, ip_ref, NULL);
    if(ip_var == NULL) {
        ip_var = amxc_var_get_key(dns_servers, ip_ref, AMXC_VAR_FLAG_DEFAULT);
    }

    ip_var_list = (amxc_llist_t*) amxc_var_constcast(amxc_llist_t, ip_var);

    amxc_llist_for_each(it, ip_dns_servers_list) {
        amxc_llist_it_take(it);
        amxc_llist_append(ip_var_list, it);
    }

    amxc_llist_delete(&ip_dns_servers_list, NULL);
    amxc_var_clean(&ip_dns_servers_var);
    free(ip_ref);
    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief Function that sets/unsets the DNS mode and servers in the tr181-dns plugin.
 *
 * @param interface Interface of the wan-manager that contains the dns servers, can be NULL.
 * @param dns_mode The dns mode in which the tr181-dns needs to be set.
 * @param ipv4 The IP version of the servers. This helps the function to only set specific dns servers depending on the ip version.
 * @param enable Adds the servers or disables them by deleting them.
 * @return amxd_status_t
 */
static amxd_status_t dns_server_toggle(amxd_object_t* interface, const char* dns_mode, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t args;
    amxc_var_t ret;
    dns_mode_t dns = string_to_dns_mode(dns_mode);

    amxc_var_init(&args);
    amxc_var_init(&ret);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);

    when_str_empty_trace(dns_mode, exit, ERROR, "Empty 'dns_mode' parameter");

    if((interface != NULL) && (dns == DNS_STATIC)) {
        amxc_var_t* dns_servers;

        if(enable) {
            amxc_var_add_key(cstring_t, &args, "Mode", dns_mode);
        } else {
            amxc_var_add_key(cstring_t, &args, "Type", dns_mode);
        }
        dns_servers = amxc_var_add_key(amxc_htable_t, &args, "DNSServers", NULL);
        dns_server_build_args(dns_servers, interface, true);  //ipv4 dns servers
        dns_server_build_args(dns_servers, interface, false); //ipv6 dns servers
    } else {
        if(enable) {
            amxc_var_add_key(cstring_t, &args, "Mode", dns_mode);
        }
    }

    if(enable) {
        rc = amxb_call(dns_get_context(), "Device.DNS.", "SetMode", &args, &ret, 5);
        when_failed_trace(rc, exit, ERROR, "Failed to set DNS mode in tr181-dns datamodel");
    } else if((interface != NULL) && (dns == DNS_STATIC)) {
        rc = amxb_call(dns_get_context(), "Device.DNS.", "DeleteForwardings", &args, &ret, 5);
        when_failed_trace(rc, exit, ERROR, "Failed to delete DNS mode in tr181-dns datamodel");
    }

exit:
    amxc_var_clean(&ret);
    amxc_var_clean(&args);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dns_mode_set(amxd_object_t* wan_mode, const char* dns_mode) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    dns_mode_t dns = DNS_NONE;

    when_str_empty_trace(dns_mode, exit, ERROR, "Empty 'dns_mode' parameter");

    dns = string_to_dns_mode(dns_mode);

    if(dns == DNS_NONE) {
        SAH_TRACEZ_ERROR(ME, "DNS mode not recognized, not applying changes");
        goto exit;
    }

    if(dns == DNS_STATIC) {

        amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {

            amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
            rc = dns_server_toggle(interface, dns_mode, true);
            when_failed_trace(rc, exit, ERROR, "Could not toggle dns servers for ipv6/ipv4");
        }
    } else {
        rc = dns_server_toggle(NULL, dns_mode, true);
        when_failed_trace(rc, exit, ERROR, "Could not toggle dns servers for ipv6/ipv4");
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t dns_mode_unset(amxd_object_t* wan_mode, const char* dns_mode) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;

    when_null_trace(wan_mode, exit, ERROR, "No wan mode reference provided, could not unset the previous dns mode");
    when_str_empty_trace(dns_mode, exit, ERROR, "No dns mode provided, could not unset previous dns mode");

    rc = amxd_status_ok;

    if(string_to_dns_mode(dns_mode) == DNS_STATIC) {

        amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {

            amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);
            rc = dns_server_toggle(interface, dns_mode, false);
            when_failed_trace(rc, exit, ERROR, "Could not disable dns servers for ipv6/ipv4");
        }
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}
