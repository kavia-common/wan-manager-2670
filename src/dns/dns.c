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
#include <amxb/amxb_types.h>

#include "ctrl/mode_ctrl.h"
#include "wan_manager_utils.h"
#include "component.h"
#include "dns/dns.h"

#define ME "wan-man"

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

amxd_status_t dns_server_del_inst(const char* dns_ip_addr, const char* dns_mode, amxd_object_t* wan_interface, bool ipv4) {
    return dns_server_mod_inst(dns_ip_addr, dns_mode, wan_interface, ipv4, true);
}

amxd_status_t dns_server_add_inst(const char* dns_ip_addr, const char* dns_mode, amxd_object_t* wan_interface, bool ipv4) {
    return dns_server_mod_inst(dns_ip_addr, dns_mode, wan_interface, ipv4, false);
}

dns_mode_t string_to_dns_mode(const char* dns_mode) {
    dns_mode_cnv_t* lookup = dns_mode_cnv;
    dns_mode_t dns = DNS_NONE;

    while(lookup->dns_str != NULL) {
        if(0 == strcmp(dns_mode, lookup->dns_str)) {
            dns = lookup->dns_mode;
            break;
        }
        lookup++;
    }
    return dns;
}

amxd_status_t dns_mode_set(amxd_object_t* wan_mode, const char* dns_mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    dns_mode_cnv_t* lookup = dns_mode_cnv;
    dns_mode_t dns = DNS_NONE;

    when_str_empty_trace(dns_mode, exit, ERROR, "Empty 'dns_mode' parameter");

    dns = string_to_dns_mode(dns_mode);

    if(dns == DNS_NONE) {
        SAH_TRACEZ_ERROR(ME, "DNS mode not recognized, not applying changes");
        goto exit;
    }

    rc = amxd_status_ok;
    lookup = dns_mode_cnv;

    while(lookup->dns_str != NULL) {
        if(dns_server_toggle(lookup->dns_str, wan_mode, ((lookup->dns_mode & dns) != 0)) != amxd_status_ok) {
            rc = amxd_status_unknown_error;
        }
        lookup++;
    }

exit:
    return rc;
}

amxd_status_t dns_mode_unset(amxd_object_t* wan_mode, const char* dns_mode) {

    amxd_status_t rc = amxd_status_ok;

    when_null_trace(wan_mode, exit, ERROR, "No wan mode reference provided, could not unset the previous dns mode");
    when_str_empty_trace(dns_mode, exit, ERROR, "No dns mode provided, could not unset previous dns mode");

    if(string_to_dns_mode(dns_mode) == DNS_STATIC) {
        if(dns_server_toggle(dns_mode, wan_mode, false) != amxd_status_ok) {
            rc = amxd_status_unknown_error;
        }
    }

exit:
    return rc;
}


amxd_status_t dns_server_toggle(const char* dns_mode, UNUSED amxd_object_t* wan_mode, bool toggle) {

    amxc_string_t dns_path;
    amxc_var_t dns_data;
    amxc_var_t* dns_instances = NULL;
    amxb_bus_ctx_t* ctx = dns_get_context();
    amxd_status_t rc = amxd_status_ok;
    int i = 0;

    amxc_string_init(&dns_path, 0);
    amxc_var_init(&dns_data);

    when_str_empty_trace(dns_mode, exit, ERROR, "Empty 'dns_mode' parameter");

    amxc_string_setf(&dns_path, "Device.DNS.Relay.Forwarding.[Type == '%s']", dns_mode);

    if(amxb_get(ctx, amxc_string_get(&dns_path, 0), 0, &dns_data, 10) != amxd_status_ok) {
        SAH_TRACEZ_ERROR(ME, "amxb_get() error, could not retrieve any dns instance");
    }
    dns_instances = amxc_var_get_first(&dns_data);
    when_null_trace(dns_instances, exit, ERROR, "No %s DNS server to %s", dns_mode, toggle ? "enable":"disable");

    amxc_var_for_each(server, dns_instances) {
        i++;
        SAH_TRACEZ_INFO(ME, "%s %d %s DNS servers", toggle ? "Enabling":"Disabling", i, dns_mode);
        if(component_set_bool(amxc_var_key(server), ctx, "Enable", toggle) != amxd_status_ok) {
            rc = amxd_status_unknown_error;
        }
    }
    when_failed(rc, exit);

    if(string_to_dns_mode(dns_mode) == DNS_STATIC) {

        amxd_object_for_each(instance, it, amxd_object_findf(wan_mode, ".Intf.")) {
            amxd_object_t* interface = amxc_container_of(it, amxd_object_t, it);

            // IPv4 DNS servers add/delete
            dns_servers_setup(interface, dns_mode, true, toggle);

            // IPv6 DNS servers add/delete
            dns_servers_setup(interface, dns_mode, false, toggle);
        }
    }

exit:
    amxc_string_clean(&dns_path);
    amxc_var_clean(&dns_data);
    return rc;
}

amxd_status_t dns_server_mod_inst(const char* dns_ip_addr, const char* dns_mode, amxd_object_t* wan_interface, bool ipv4, bool rm) {

    amxd_status_t rc = amxd_status_unknown_error;
    amxb_bus_ctx_t* ctx = dns_get_context();
    amxc_string_t alias;
    amxc_string_t dns_path;
    amxc_var_t dns_data;
    amxc_var_t parameter;
    amxc_var_t* tmp = NULL;
    char* ip_ref = amxd_object_get_value(cstring_t, wan_interface, ipv4 ? "IPv4Reference":"IPv6Reference", NULL);
    int index = 0;
    char* path = NULL;
    const char* tag = "wanmngr";

    amxc_var_init(&dns_data);
    amxc_var_init(&parameter);
    amxc_string_init(&alias, 0);
    amxc_string_init(&dns_path, 0);

    when_null_trace(ip_ref, exit, ERROR, "Could not retrieve the wan mode interface data");
    when_null_trace(dns_ip_addr, exit, ERROR, "No DNS ip address given");

    // If rm == true, this part will remove the related dns server from the plugin using the Tag parameter
    if(rm) {
        amxc_string_setf(&dns_path, "Device.DNS.Relay.Forwarding.[Tag == '%s' && DNSServer == '%s']", tag, dns_ip_addr);
        path = component_del_instance(amxc_string_get(&dns_path, 0), ctx);

        if(path != NULL) {
            rc = amxd_status_ok;
        }
        goto exit;
    }

    /**
     * Creation of the Alias with the following rule:
     * Alias = "dns_mode"+"-"+"index"
     *
     * Example: dns_mode = Static -> Alias = static-3 if 2 static instances already exist
     */
    amxc_string_set(&alias, dns_mode);
    amxc_string_to_lower(&alias);

    index++;
    amxc_string_setf(&dns_path, "Device.DNS.Relay.Forwarding.[Alias == '%s-%d']", amxc_string_get(&alias, 0), index);
    amxb_get(ctx, amxc_string_get(&dns_path, 0), 0, &dns_data, 10);

    while((amxc_var_get_first(amxc_var_get_first(&dns_data)) != NULL)) {
        index++;
        amxc_string_setf(&dns_path, "Device.DNS.Relay.Forwarding.[Alias == '%s-%d']", amxc_string_get(&alias, 0), index);
        amxb_get(ctx, amxc_string_get(&dns_path, 0), 0, &dns_data, 10);
    }

    //Creating the Alias and adding the instance
    amxc_string_appendf(&alias, "-%d", index);

    amxc_var_set_type(&parameter, AMXC_VAR_ID_HTABLE);
    tmp = amxc_var_add_new_key(&parameter, "Alias");
    amxc_var_push(cstring_t, tmp, amxc_string_take_buffer(&alias));
    amxc_var_add_key(cstring_t, &parameter, "DNSServer", dns_ip_addr);
    amxc_var_add_key(cstring_t, &parameter, "Interface", ip_ref);
    amxc_var_add_key(cstring_t, &parameter, "Type", dns_mode);
    amxc_var_add_key(cstring_t, &parameter, "Tag", tag);
    amxc_var_add_key(bool, &parameter, "Enable", true);

    //Add the instance to the datamodel
    path = component_add_instance("Device.DNS.Relay.Forwarding.", &parameter, ctx);
    when_null_trace(path, exit, ERROR, "Could not add %s to the dns plugin.", GET_CHAR(tmp, NULL));

    if(path != NULL) {
        rc = amxd_status_ok;
    }

exit:
    amxc_var_clean(&dns_data);
    amxc_var_clean(&parameter);
    amxc_string_clean(&alias);
    amxc_string_clean(&dns_path);
    free(ip_ref);
    free(path);
    return rc;
}

amxd_status_t dns_servers_setup(amxd_object_t* interface, const char* dns_mode, bool ipv4, bool toggle) {

    const char* ip_ref = ipv4 ? ".IPv4Address.":".IPv6Address.";
    amxd_status_t rc = amxd_status_ok;
    amxb_bus_ctx_t* ctx = dns_get_context();

    when_str_empty_trace(dns_mode, exit, ERROR, "No DNS mode provided");
    when_null_trace(interface, exit, ERROR, "No wanmode interface provided");

    amxd_object_iterate(instance, list, amxd_object_findf(interface, "%s", ip_ref)) {
        const char* dns_server_ip = NULL;
        amxc_var_t ip_dns_servers_list;
        amxd_object_t* ip = amxc_container_of(list, amxd_object_t, it);

        amxc_var_init(&ip_dns_servers_list);

        dns_server_ip = GET_CHAR(amxd_object_get_param_value(ip, "DNSServers"), NULL);
        amxc_var_set(cstring_t, &ip_dns_servers_list, dns_server_ip);
        amxc_var_cast(&ip_dns_servers_list, AMXC_VAR_ID_LIST);

        amxc_var_for_each(addr, &ip_dns_servers_list) {
            amxc_var_t dns_data;
            amxc_string_t dns_path;

            amxc_string_init(&dns_path, 0);
            amxc_var_init(&dns_data);

            amxc_string_setf(&dns_path, "Device.DNS.Relay.Forwarding.[DNSServer == '%s']", amxc_var_constcast(cstring_t, addr));
            amxb_get(ctx, amxc_string_get(&dns_path, 0), 0, &dns_data, 10);

            //Check if the DNSServer does not exist in the DNS plugin
            if((amxc_var_get_first(amxc_var_get_first(&dns_data)) == NULL) && toggle) {

                //Add the DNS server to the dm of the plugin
                SAH_TRACEZ_INFO(ME, "Adding this address: %s to the DNS plugin", amxc_var_constcast(cstring_t, addr));
                if(dns_server_add_inst(amxc_var_constcast(cstring_t, addr), dns_mode, interface, ipv4) != amxd_status_ok) {
                    rc = amxd_status_unknown_error;
                }
            } else if((amxc_var_get_first(amxc_var_get_first(&dns_data)) != NULL) && !toggle) {

                //Remove the DNS Server from the dm of the plugin
                SAH_TRACEZ_INFO(ME, "Removing this address: %s from the DNS plugin", amxc_var_constcast(cstring_t, addr));
                if(dns_server_del_inst(amxc_var_constcast(cstring_t, addr), dns_mode, interface, ipv4) != amxd_status_ok) {
                    rc = amxd_status_unknown_error;
                }
            }

            amxc_var_clean(&dns_data);
            amxc_string_clean(&dns_path);
        }

        amxc_var_clean(&ip_dns_servers_list);
    }

exit:
    return rc;
}
