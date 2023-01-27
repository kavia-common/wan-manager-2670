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

amxb_bus_ctx_t* ip_get_context(void) {
    return amxb_be_who_has("IP.");
}

amxb_bus_ctx_t* dhcpv4_get_context(void) {
    return amxb_be_who_has("DHCPv4.");
}

amxb_bus_ctx_t* dhcpv6_get_context(void) {
    return amxb_be_who_has("DHCPv6.");
}

amxb_bus_ctx_t* ppp_get_context(void) {
    return amxb_be_who_has("PPP.");
}

amxb_bus_ctx_t* routing_get_context(void) {
    return amxb_be_who_has("Routing.");
}

amxb_bus_ctx_t* dns_get_context(void) {
    return amxb_be_who_has("DNS.");
}

amxb_bus_ctx_t* ethernet_get_context(void) {
    return amxb_be_who_has("Ethernet.");
}

amxb_bus_ctx_t* logical_get_context(void) {
    return amxb_be_who_has("Logical.");
}

amxb_bus_ctx_t* neighbor_discovery_get_context(void) {
    return amxb_be_who_has("NeighborDiscovery.");
}

amxb_bus_ctx_t* dslite_get_context(void) {
    return amxb_be_who_has("DSLite.");
}

amxd_status_t ipv6_addr_toggle(const char* intf_path, amxc_var_t* ip_addr, const char* addr_type, bool enable) {
    amxd_status_t rc = amxd_status_ok;
    amxc_var_t params;
    amxc_string_t addr_path;
    char* path = NULL;
    const char* ip_param = "IPv6Address";
    const char* ip_v = "6";
    const char* gua_ra = "GUA_RA";
    const char* alias = "wan-mngr";

    amxc_string_init(&addr_path, 0);
    amxc_var_init(&params);

    when_str_empty_trace(intf_path, exit, ERROR, "Interface path for the ipv6 address is empty");
    when_str_empty_trace(addr_type, exit, ERROR, "Addressing type of the ipv6 address is empty");

    if(enable) {

        amxc_string_setf(&addr_path, "%sIPv%sAddress.[Alias == '%s']", intf_path, ip_v, alias);
        path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));

        //Setting up the new ipv6 address
        amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, &params, "IPAddress", GET_CHAR(ip_addr, ip_param));
        amxc_var_add_key(cstring_t, &params, "Origin", addr_type);
        amxc_var_add_key(bool, &params, "Enable", false);

        if(path == NULL) {

            //Creating an ipv6 instance
            amxc_var_add_key(cstring_t, &params, "Alias", alias);
            amxc_string_setf(&addr_path, "%sIPv%sAddress.", intf_path, ip_v);
            path = component_add_instance(amxc_string_get(&addr_path, 0), &params, ip_get_context());

            if(path == NULL) {
                rc = amxd_status_unknown_error;
                SAH_TRACEZ_ERROR(ME, "Could not add a static IPv6 instance to IP-Manager");
                goto exit;
            }
        } else {

            //Setting the parameters in the right ipv6 instance
            rc = component_set_params(path, ip_get_context(), &params);
            when_failed_trace(rc, exit, ERROR, "Could not update the Static IPv6 instance to %s", path);
        }

        //Activating the custom IPv6 address
        rc = component_set_enable(path, ip_get_context(), true);
        when_failed_trace(rc, exit, ERROR, "Could not enable %s", path);

        //Deactivating the GUA_RA
        amxc_string_setf(&addr_path, "%sIPv%sAddress.[Alias == '%s']", intf_path, ip_v, gua_ra);
        free(path);
        path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));

        rc = component_set_enable(path, ip_get_context(), !enable);
        when_failed_trace(rc, exit, ERROR, "Could not disable the %s IPv6 address.", gua_ra);
    } else {

        amxc_string_setf(&addr_path, "%sIPv%sAddress.[Alias == '%s' && Origin == '%s']", intf_path, ip_v, alias, addr_type);

        //Deleting the ipv6 instance
        rc = component_del_instance(amxc_string_get(&addr_path, 0), ip_get_context());
        when_failed_trace(rc, exit, ERROR, "Could not delete instance %s", amxc_string_get(&addr_path, 0));

        //Activating the GUA_RA
        amxc_string_setf(&addr_path, "%sIPv%sAddress.[Alias == '%s']", intf_path, ip_v, gua_ra);
        path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));

        rc = component_set_enable(path, ip_get_context(), !enable);
        when_failed_trace(rc, exit, ERROR, "Could not enable the %s IPv6 address.", gua_ra);
    }
exit:
    free(path);
    amxc_var_clean(&params);
    amxc_string_clean(&addr_path);
    return rc;
}

amxd_status_t ipv4_addr_toggle(const char* intf_path, amxc_var_t* ip_addr, const char* addr_type) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t addr_path;
    amxc_var_t params;
    char* path = NULL;
    const char* alias = "wan";
    const char* ip_v = "4";
    const char* ip_param = "IPv4Address";

    amxc_string_init(&addr_path, 0);
    amxc_var_init(&params);

    when_str_empty_trace(intf_path, exit, ERROR, "Interface path for the ipv4 address is empty");
    when_str_empty_trace(addr_type, exit, ERROR, "Addressing type of the ipv4 address is empty");

    amxc_string_setf(&addr_path, "%sIPv%sAddress.[Alias == '%s']", intf_path, ip_v, alias);
    path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));

    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &params, "AddressingType", addr_type);

    if(ip_addr != NULL) {
        amxc_var_add_key(cstring_t, &params, "IPAddress", GET_CHAR(ip_addr, ip_param));
        amxc_var_add_key(cstring_t, &params, "SubnetMask", GET_CHAR(ip_addr, "SubnetMask"));
    } else {
        amxc_var_add_key(cstring_t, &params, "IPAddress", "");
        amxc_var_add_key(cstring_t, &params, "SubnetMask", "");
    }

    rc = component_set_params(path, ip_get_context(), &params);
    when_failed_trace(rc, exit, ERROR, "Could not fill %s into the datamodel", path);

    rc = component_set_enable(path, ip_get_context(), true);
    when_failed_trace(rc, exit, ERROR, "Could not %s %s", "enable", path);

exit:
    free(path);
    amxc_var_clean(&params);
    amxc_string_clean(&addr_path);
    return rc;
}

amxd_status_t routing_default_route_set_origin(const char* ip_path, const char* routing_origin, const char* ip_addr) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t path;
    amxc_var_t params;
    const char* route_path = NULL;

    amxc_string_init(&path, 0);
    amxc_var_init(&params);

    when_str_empty_trace(routing_origin, exit, ERROR, "Routing Origin parameter empty");

    amxc_string_setf(&path, "Device.Routing.Router.1.IPv4Forwarding.[Interface=='%s' && DestIPAddress=='0.0.0.0' && DestSubnetMask=='0.0.0.0'].", ip_path);
    route_path = amxc_string_get(&path, 0);

    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &params, "Origin", routing_origin);

    if((ip_addr != NULL) && (strcmp(routing_origin, ROUTING_ORIGIN_STATIC) == 0)) {
        amxc_var_add_key(cstring_t, &params, "GatewayIPAddress", ip_addr);
    }

    rc = component_set_params(route_path, routing_get_context(), &params);
    when_failed_trace(rc, exit, ERROR, "Could not fill %s into the datamodel", route_path);

exit:
    amxc_var_clean(&params);
    amxc_string_clean(&path);
    return rc;
}

amxd_status_t routing_default_ipv6_route_mod_inst(const char* routing_origin, const char* next_hop, const char* ip_intf, bool enable) {
    amxd_status_t rc = amxd_status_ok;
    const char* id = "wan-mngr";
    char* path = NULL;
    static int my_index = 0;
    amxc_string_t route_path;
    amxc_string_t alias;
    amxc_var_t* tmp = NULL;

    amxc_string_init(&route_path, 0);
    amxc_string_init(&alias, 0);

    if(enable) {

        amxc_var_t params;

        when_str_empty(routing_origin, exit);
        when_str_empty(next_hop, exit);

        amxc_var_init(&params);
        amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

        amxc_var_add_key(cstring_t, &params, "DestIPPrefix", "::/0");
        amxc_var_add_key(bool, &params, "Enable", true);
        amxc_var_add_key(cstring_t, &params, "NextHop", next_hop);
        amxc_var_add_key(cstring_t, &params, "Origin", routing_origin);
        amxc_var_add_key(cstring_t, &params, "Interface", ip_intf);

        amxc_string_setf(&route_path, "Device.Routing.Router.1.IPv6Forwarding.[Alias=='%s'].", id);
        path = component_get_path_instance(routing_get_context(), amxc_string_get(&route_path, 0));
        while(path != NULL) {

            my_index++;
            amxc_string_setf(&route_path, "Device.Routing.Router.1.IPv6Forwarding.[Alias=='%s-%d'].", id, my_index);
            free(path);
            path = component_get_path_instance(routing_get_context(), amxc_string_get(&route_path, 0));
        }

        if(my_index == 0) {
            amxc_string_setf(&alias, "%s", id);
        } else {
            amxc_string_setf(&alias, "%s-%d", id, my_index);
        }

        tmp = amxc_var_add_new_key(&params, "Alias");
        amxc_var_push(cstring_t, tmp, amxc_string_take_buffer(&alias));
        free(path);
        path = component_add_instance("Device.Routing.Router.1.IPv6Forwarding.", &params, routing_get_context());

        if(path == NULL) {
            rc = amxd_status_unknown_error;
            SAH_TRACEZ_ERROR(ME, "Could not add an IPv6Forwarding instance with alias %s to routing manager", amxc_string_get(&alias, 0));
        }
        amxc_var_clean(&params);
    } else {

        if(my_index == 0) {
            amxc_string_setf(&alias, "%s", id);
        } else {
            amxc_string_setf(&alias, "%s-%d", id, my_index);
        }

        amxc_string_setf(&route_path, "Device.Routing.Router.1.IPv6Forwarding.[Alias=='%s'].", amxc_string_get(&alias, 0));
        component_del_instance(amxc_string_get(&route_path, 0), routing_get_context());

        my_index = 0;
    }

exit:
    amxc_string_clean(&alias);
    amxc_string_clean(&route_path);
    free(path);
    return rc;
}

amxd_status_t nd_interface_setting_toggle(const char* intf_alias, bool enable) {
    amxc_string_t nd_path;
    amxd_status_t rc = amxd_status_ok;

    amxc_string_init(&nd_path, 0);
    amxc_string_setf(&nd_path, "Device.NeighborDiscovery.InterfaceSetting.[Alias == 'cpe-%s']", intf_alias);

    rc = component_set_enable(amxc_string_get(&nd_path, 0), neighbor_discovery_get_context(), enable);
    when_failed_trace(rc, exit, ERROR, "Could not %s %s in Device.NeighborDiscovery.InterfaceSetting", enable? "enable":"disable", intf_alias);

exit:
    amxc_string_clean(&nd_path);
    return rc;
}

/**
 * @brief Function that returns the path of a Routing.RouteInformation.InterfaceSetting. instance
 * if the interface path exists. If the interface path does not exist, the function creates a blank
 * Routing.RouteInformation.InterfaceSetting. instance while also providing the path to it.
 *
 * @param intf_path
 * @return The path to the found/created instance of Routing.RouteInformation.InterfaceSetting., NULL if it fails to create the instance.
 */
char* routing_get_interfacesetting(const char* intf_path) {
    amxb_bus_ctx_t* ctx = routing_get_context();
    amxc_string_t test_path;
    amxc_var_t parameter;
    amxc_var_t* tmp = NULL;
    amxc_string_t alias;
    char* path = NULL;
    const char* tag = "Wan-Manager";
    static int routing_nr_inst = 0;

    amxc_string_init(&test_path, 0);
    amxc_string_init(&alias, 0);
    amxc_var_init(&parameter);

    when_null_trace(intf_path, exit, ERROR, "Null interface path provided for the Routing mananger");

    amxc_string_setf(&test_path, "Device.Routing.RouteInformation.InterfaceSetting.[Interface == '%s']", intf_path);

    path = component_get_path_instance(ctx, amxc_string_get(&test_path, 0));

    if(path == NULL) {
        amxc_string_setf(&test_path, "Device.Routing.RouteInformation.InterfaceSetting.[Interface == '']");
        path = component_get_path_instance(ctx, amxc_string_get(&test_path, 0));

        // Create the instance if none are found in the routing manager
        if(path == NULL) {
            routing_nr_inst++;
            amxc_string_setf(&alias, "%s-%d", tag, routing_nr_inst);

            amxc_var_set_type(&parameter, AMXC_VAR_ID_HTABLE);
            tmp = amxc_var_add_new_key(&parameter, "Alias");
            amxc_var_push(cstring_t, tmp, amxc_string_take_buffer(&alias));
            amxc_var_add_key(cstring_t, &parameter, "Interface", "");
            amxc_var_add_key(cstring_t, &parameter, "PreferredRouteFlag", "High");

            //Add the instance to the datamodel
            path = component_add_instance("Device.Routing.RouteInformation.InterfaceSetting.", &parameter, ctx);
            when_null_trace(path, exit, ERROR, "Could not add a blank InterfaceSetting to the Routing plugin");
        }
    }

exit:
    amxc_var_clean(&parameter);
    amxc_string_clean(&test_path);
    amxc_string_clean(&alias);
    return path;
}

// Add a string to a list only if that list does not yet contain that string
void add_str_to_list(amxc_var_t* list, const char* str) {
    bool found = false;

    when_null(list, exit);
    when_str_empty(str, exit);
    amxc_var_for_each(var, list) {
        if(strcmp(GET_CHAR(var, NULL), str) == 0) {
            found = true;
            break;
        }
    }
    if(!found) {
        amxc_var_add(cstring_t, list, str);
    }
exit:
    return;
}

// Remove all occurrences of a string from a list
void remove_str_from_list(amxc_var_t* list, const char* str) {
    when_null(list, exit);
    when_str_empty(str, exit);

    amxc_var_for_each(var, list) {
        if(strcmp(GET_CHAR(var, NULL), str) == 0) {
            amxc_var_delete(&var);
        }
    }
exit:
    return;
}

char* create_logical_path(const char* intf_name) {
    char* path = NULL;
    amxc_string_t logical_intf;

    amxc_string_init(&logical_intf, 0);
    amxc_string_setf(&logical_intf, "Device.Logical.Interface.%s.", intf_name);
    path = amxc_string_take_buffer(&logical_intf);
    amxc_string_clean(&logical_intf);

    return path;
}