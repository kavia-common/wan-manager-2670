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

#include "dm_wan-manager.h"
#include "dm_wan_mode.h"
#include "component.h"

#include "wan_manager_utils.h"
#define ME "wan-man"

amxb_bus_ctx_t* ip_get_context(void) {
    return amxb_be_who_has("IP.");
}

amxb_bus_ctx_t* dhcpv4_get_context(void) {
    return amxb_be_who_has("Device.DHCPv4.");
}

amxb_bus_ctx_t* dhcpv6_get_context(void) {
    return amxb_be_who_has("Device.DHCPv6.");
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
    return amxb_be_who_has("Device.Logical.");
}

amxb_bus_ctx_t* neighbor_discovery_get_context(void) {
    return amxb_be_who_has("NeighborDiscovery.");
}

amxb_bus_ctx_t* dslite_get_context(void) {
    return amxb_be_who_has("DSLite.");
}

amxb_bus_ctx_t* pcp_get_context(void) {
    return amxb_be_who_has("PCP.");
}

amxb_bus_ctx_t* xpon_get_context(void) {
    return amxb_be_who_has("XPON.");
}

amxd_status_t ipv6_addr_toggle(const char* intf_path, amxc_var_t* ip_addr, const char* addr_type, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_ok;
    amxc_var_t params;
    amxc_string_t addr_path;
    char* path = NULL;
    const char* ip_param = "IPv6Address";
    const char* gua_ra = "GUA_RA";
    const char* alias = "wan-mngr";

    amxc_string_init(&addr_path, 0);
    amxc_var_init(&params);

    when_str_empty_trace(intf_path, exit, ERROR, "Interface path for the ipv6 address is empty");
    when_str_empty_trace(addr_type, exit, ERROR, "Addressing type of the ipv6 address is empty");

    if(enable) {
        amxc_string_setf(&addr_path, "%sIPv6Address.[Alias == '%s']", intf_path, alias);
        path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));

        //Setting up the new ipv6 address
        amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, &params, "IPAddress", GET_CHAR(ip_addr, ip_param));
        amxc_var_add_key(cstring_t, &params, "Origin", addr_type);
        amxc_var_add_key(bool, &params, "Enable", false);

        if(path == NULL) {
            //Creating an ipv6 instance
            amxc_var_add_key(cstring_t, &params, "Alias", alias);
            amxc_string_setf(&addr_path, "%sIPv6Address.", intf_path);
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
        free(path);

        //Deactivating the GUA_RA
        amxc_string_setf(&addr_path, "%sIPv6Address.[Alias == '%s']", intf_path, gua_ra);
        path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));
        rc = component_set_enable(path, ip_get_context(), !enable);
        when_failed_trace(rc, exit, ERROR, "Could not disable the %s IPv6 address.", gua_ra);
    } else {
        //Deleting the ipv6 instance
        amxc_string_setf(&addr_path, "%sIPv6Address.[Alias == '%s' && Origin == 'Static']", intf_path, alias);
        rc = component_del_instance(amxc_string_get(&addr_path, 0), ip_get_context());
        when_failed_trace(rc, exit, ERROR, "Could not delete instance %s", amxc_string_get(&addr_path, 0));

        //Activating the GUA_RA
        amxc_string_setf(&addr_path, "%sIPv6Address.[Alias == '%s']", intf_path, gua_ra);
        path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));

        rc = component_set_enable(path, ip_get_context(), !enable);
        when_failed_trace(rc, exit, ERROR, "Could not enable the %s IPv6 address.", gua_ra);
    }

exit:
    free(path);
    amxc_var_clean(&params);
    amxc_string_clean(&addr_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t ipv4_addr_toggle(const char* intf_path, amxc_var_t* ip_addr, const char* addr_type, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_t addr_path;
    amxc_var_t params;
    char* path = NULL;
    const char* alias = "primary";

    amxc_string_init(&addr_path, 0);
    amxc_var_init(&params);

    when_str_empty_trace(intf_path, exit, ERROR, "Interface path for the ipv4 address is empty");
    when_str_empty_trace(addr_type, exit, ERROR, "Addressing type of the ipv4 address is empty");

    amxc_string_setf(&addr_path, "%sIPv4Address.[Alias == '%s']", intf_path, alias);
    path = component_get_path_instance(ip_get_context(), amxc_string_get(&addr_path, 0));
    when_str_empty_trace(path, exit, ERROR, "Failed to find path '%s'", amxc_string_get(&addr_path, 0));

    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &params, "AddressingType", addr_type);

    if(ip_addr != NULL) {
        amxc_var_add_key(cstring_t, &params, "IPAddress", GET_CHAR(ip_addr, "IPv4Address"));
        amxc_var_add_key(cstring_t, &params, "SubnetMask", GET_CHAR(ip_addr, "SubnetMask"));
    }

    if(!enable) {
        rc = component_set_enable(path, ip_get_context(), false);
        when_failed_trace(rc, exit, ERROR, "Could not disable %s", path);
    }

    rc = component_set_params(path, ip_get_context(), &params);
    when_failed_trace(rc, exit, ERROR, "Could not fill %s into the datamodel", path);

    if(enable) {
        rc = component_set_enable(path, ip_get_context(), true);
        when_failed_trace(rc, exit, ERROR, "Could not %s %s", "enable", path);
    }
exit:
    free(path);
    amxc_var_clean(&params);
    amxc_string_clean(&addr_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

static void get_prefixes_from_interfaces(const char* interfaces, const char* old_reference_path, amxc_var_t* prefixes) {
    SAH_TRACEZ_IN(ME);
    int rc = -1;
    amxc_string_t interfaces_string;
    amxc_string_t search_path;
    amxc_llist_t interfaces_list;

    amxc_string_init(&interfaces_string, 0);
    amxc_string_init(&search_path, 0);
    amxc_llist_init(&interfaces_list);

    amxc_string_set(&interfaces_string, interfaces);
    amxc_var_set_type(prefixes, AMXC_VAR_ID_LIST);

    rc = amxc_string_split_to_llist(&interfaces_string, &interfaces_list, ',');
    when_failed_trace(rc, exit, ERROR, "Failed to split comma separated list");

    amxc_llist_for_each(it, &interfaces_list) {
        int rv = -1;
        amxc_string_t* interface_path = amxc_container_of(it, amxc_string_t, it);
        amxc_var_t* tmp = NULL;
        amxc_var_t ret;
        amxc_var_init(&ret);

        amxc_string_setf(&search_path, "%s.IPv6Prefix.[ParentPrefix starts with '%sIPv6Prefix.'].ParentPrefix", amxc_string_get(interface_path, 0), old_reference_path);
        rv = amxb_get(ip_get_context(), amxc_string_get(&search_path, 0), 0, &ret, 5);
        if(rv == AMXB_STATUS_OK) {
            tmp = amxc_var_add_new(prefixes);
            amxc_var_move(tmp, GETI_ARG(&ret, 0));
        } else {
            SAH_TRACEZ_ERROR(ME, "Failed to get the ParentPrefixes for %s, error '%d'", amxc_string_get(interface_path, 0), rv);
        }

        amxc_var_clean(&ret);
    }

exit:
    amxc_string_clean(&interfaces_string);
    amxc_string_clean(&search_path);
    amxc_llist_clean(&interfaces_list, amxc_string_list_it_free);
    SAH_TRACEZ_OUT(ME);
    return;
}

static void convert_prefixes_data(const char* interfaces, const char* old_reference_path, const char* new_reference_path, amxc_var_t* data) {
    SAH_TRACEZ_IN(ME);
    amxc_var_t ret;
    amxc_string_t new_path;

    amxc_var_set_type(data, AMXC_VAR_ID_LIST);
    amxc_var_init(&ret);
    amxc_string_init(&new_path, 0);

    get_prefixes_from_interfaces(interfaces, old_reference_path, &ret);

    amxc_var_for_each(interface, &ret) {
        amxc_var_for_each(instance, interface) {
            amxc_var_t* new_instance_data = amxc_var_add(amxc_htable_t, data, NULL);
            amxc_var_t* params = amxc_var_add_key(amxc_htable_t, new_instance_data, "parameters", NULL);
            const char* prefix_path = GET_CHAR(instance, "ParentPrefix");

            amxc_string_setf(&new_path, "%s", prefix_path);
            amxc_string_replace(&new_path, old_reference_path, new_reference_path, 1);

            amxc_var_add_key(cstring_t, new_instance_data, "path", amxc_var_key(instance));
            amxc_var_add_key(cstring_t, params, "ParentPrefix", amxc_string_get(&new_path, 0));
        }
    }

    amxc_string_clean(&new_path);
    amxc_var_clean(&ret);
    SAH_TRACEZ_OUT(ME);
}

int ip_parent_prefix_toggle(const char* interfaces, const char* old_reference_path, const char* new_reference_path) {
    SAH_TRACEZ_IN(ME);
    int rv = -1;

    when_str_empty_trace(new_reference_path, exit, ERROR, "New IPv6Reference not provided");
    rv = 0;
    when_str_empty_trace(old_reference_path, exit, INFO, "No old IPv6Reference provided");
    when_str_empty_trace(interfaces, exit, INFO, "No deferred interfaces");

    if(strcmp(old_reference_path, new_reference_path) != 0) {
        amxc_var_t req_paths;
        amxc_var_init(&req_paths);

        convert_prefixes_data(interfaces, old_reference_path, new_reference_path, &req_paths);
        rv = amxb_set_multiple(ip_get_context(), 0, &req_paths, NULL, 5);

        amxc_var_clean(&req_paths);
        when_failed_trace(rv, exit, ERROR, "Failed to set new parent prefixes for the deferred IPv6 interfaces");
    }

exit:
    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t routing_default_route_set_origin(const char* route_path, const char* ip_path, const char* routing_origin, const char* ip_addr) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_var_t params;

    amxc_var_init(&params);

    when_str_empty_trace(route_path, exit, ERROR, "No forwarding instance specified for default route");
    when_str_empty_trace(routing_origin, exit, ERROR, "Routing Origin parameter empty");

    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &params, "Origin", routing_origin);
    amxc_var_add_key(cstring_t, &params, "Interface", ip_path);

    if((ip_addr != NULL) && (strcmp(routing_origin, ROUTING_ORIGIN_STATIC) == 0)) {
        amxc_var_add_key(cstring_t, &params, "GatewayIPAddress", ip_addr);
    }

    rc = component_set_params(route_path, routing_get_context(), &params);
    when_failed_trace(rc, exit, ERROR, "Could not fill %s into the datamodel", route_path);

exit:
    amxc_var_clean(&params);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t routing_default_ipv6_route_mod_inst(const char* routing_origin, const char* next_hop, const char* ip_intf, bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_ok;
    const char* id = "wan-mngr";
    char* path = NULL;
    static int my_index = 0;
    amxc_var_t* tmp = NULL;
    amxc_string_t route_path;
    amxc_string_t alias;
    amxc_var_t params;

    amxc_string_init(&route_path, 0);
    amxc_string_init(&alias, 0);
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    if(enable) {
        when_str_empty(routing_origin, exit);
        when_str_empty(next_hop, exit);

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
    amxc_var_clean(&params);
    amxc_string_clean(&alias);
    amxc_string_clean(&route_path);
    free(path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t nd_interface_setting_toggle(const char* intf_alias, bool enable) {
    SAH_TRACEZ_IN(ME);
    char* nd_path = NULL;
    amxd_status_t rc = amxd_status_ok;

    nd_path = create_neighbor_discovery_path(intf_alias);

    rc = component_set_enable(nd_path, neighbor_discovery_get_context(), enable);
    when_failed_trace(rc, exit, ERROR, "Could not %s %s in Device.NeighborDiscovery.InterfaceSetting", enable ? "enable" : "disable", intf_alias);

exit:
    free(nd_path);
    SAH_TRACEZ_OUT(ME);
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
    SAH_TRACEZ_IN(ME);
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
    SAH_TRACEZ_OUT(ME);
    return path;
}

// Add a string to a list only if that list does not yet contain that string
void add_str_to_list(amxc_var_t* list, const char* str) {
    SAH_TRACEZ_IN(ME);
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
    SAH_TRACEZ_OUT(ME);
    return;
}

// Remove all occurrences of a string from a list
void remove_str_from_list(amxc_var_t* list, const char* str) {
    SAH_TRACEZ_IN(ME);
    when_null(list, exit);
    when_str_empty(str, exit);

    amxc_var_for_each(var, list) {
        if(strcmp(GET_CHAR(var, NULL), str) == 0) {
            amxc_var_delete(&var);
        }
    }
exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

char* create_logical_path(const char* intf_name) {
    SAH_TRACEZ_IN(ME);
    char* path = NULL;
    amxc_string_t logical_intf;

    amxc_string_init(&logical_intf, 0);
    amxc_string_setf(&logical_intf, "Device.Logical.Interface.%s.", intf_name);
    path = amxc_string_take_buffer(&logical_intf);
    amxc_string_clean(&logical_intf);

    SAH_TRACEZ_OUT(ME);
    return path;
}

char* create_neighbor_discovery_path(const char* intf_alias) {
    SAH_TRACEZ_IN(ME);
    char* path = NULL;
    amxc_string_t nd_path;

    amxc_string_init(&nd_path, 0);
    amxc_string_setf(&nd_path, "Device.NeighborDiscovery.InterfaceSetting.[Alias == 'cpe-%s']", intf_alias);
    path = amxc_string_take_buffer(&nd_path);
    amxc_string_clean(&nd_path);

    SAH_TRACEZ_OUT(ME);
    return path;
}

const char* object_const_string(amxd_object_t* object, const char* name) {
    const char* value = GET_CHAR(amxd_object_get_param_value(object, name), NULL);
    return value != NULL ? value : "";
}