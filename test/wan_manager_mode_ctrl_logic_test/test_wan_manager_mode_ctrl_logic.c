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

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <string.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>
#include <amxc/amxc_macros.h>

#include "test_wan_manager_mode_ctrl_logic.h"
#include "test_utils.h"
#include "wan_manager_utils.h"

static void assert_nr_instances(const char* templ_path, uint32_t expected_nr) {
    amxd_object_t* templ_obj = amxd_dm_findf(test_get_dm(), templ_path);
    assert_non_null(templ_obj);
    assert_int_equal(amxc_llist_size(&templ_obj->instances), expected_nr);
}

static void assert_nr_active_objects(const char* search_path, uint32_t expected_nr) {
    amxc_llist_t paths;
    amxc_llist_init(&paths);
    amxd_dm_resolve_pathf(test_get_dm(), &paths, "%s.[Enable == 1].", search_path);
    assert_int_equal(amxc_llist_size(&paths), expected_nr);
    amxc_llist_clean(&paths, amxc_string_list_it_free);
}

static bool is_ppp_mode(const char* wan_mode) {
    return strcmp(wan_mode, "demo_pppmode") == 0 || strcmp(wan_mode, "demo_ppp6mode") == 0;
}

static bool is_dhcp_mode(const char* wan_mode) {
    return strcmp(wan_mode, "demo_wanmode") == 0 || strcmp(wan_mode, "demo_test") == 0;
}

static void assert_dslite(void) {
    amxd_object_t* dslite_obj = amxd_dm_findf(test_get_dm(), "DSLite.");
    amxd_object_t* pcp_obj = amxd_dm_findf(test_get_dm(), "PCP.");
    amxd_object_t* dslite_intf_obj = NULL;

    assert_non_null(dslite_obj);
    assert_true(amxd_object_get_bool(dslite_obj, "Enable", NULL));
    assert_non_null(pcp_obj);
    assert_true(amxd_object_get_bool(pcp_obj, "Enable", NULL));

    dslite_intf_obj = amxd_object_findf(dslite_obj, "InterfaceSetting.1.");
    assert_non_null(dslite_intf_obj);

}

static void assert_routing_ipv4forward(const char* ip_interface, const char* origin) {
    amxd_object_t* router = amxd_dm_findf(test_get_dm(), "Routing.Router.1.");
    amxd_object_t* router_ipv4_inst = NULL;
    amxc_var_t ipv4_params;
    amxd_status_t rc = 0;


    amxc_var_init(&ipv4_params);

    assert_non_null(router);
    assert_true(amxd_object_get_bool(router, "Enable", &rc));
    assert_int_equal(rc, 0);

    router_ipv4_inst = amxd_object_findf(router, "IPv4Forwarding.[Interface==\"Device.IP.%s.\"]", ip_interface);
    assert_non_null(router_ipv4_inst);
    amxd_object_get_params(router_ipv4_inst, &ipv4_params, amxd_dm_access_private);
    assert_true(GET_BOOL(&ipv4_params, "Enable"));
    assert_string_equal(GET_CHAR(&ipv4_params, "Origin"), origin);
    amxc_var_clean(&ipv4_params);
}

static void assert_ip_dm(const char* ip_interface, const char* ipv4_addr_alias, const char* ipv6_addr_alias, const char* addressing_type, const char* ll, const char* ipv6addr_delegate) {
    amxc_var_t ip_parameters;
    amxd_object_t* ip_dm = amxd_dm_findf(test_get_dm(), "IP.");
    amxd_object_t* ip_inst = amxd_object_findf(ip_dm, ip_interface);
    char* _ll = remove_device_prefix(ll);
    amxd_object_t* ll_inst = amxd_dm_findf(test_get_dm(), _ll);
    bool ipv4 = !str_empty(ipv4_addr_alias);
    bool ipv6 = !str_empty(ipv6_addr_alias);

    amxc_var_init(&ip_parameters);

    assert_non_null(ll_inst);

    assert_int_equal(amxd_object_get_params(ip_inst, &ip_parameters, amxd_dm_access_protected), 0);
    assert_string_equal(ipv6addr_delegate, GET_CHAR(&ip_parameters, "IPv6AddressDelegate"));
    assert_string_equal(ll, GET_CHAR(&ip_parameters, "LowerLayers"));
    assert_int_equal(1, GET_BOOL(&ip_parameters, "Enable"));

    if(ipv4) {
        amxd_object_t* ipv4_inst = NULL;
        amxc_var_t ipv4_parameters;

        amxc_var_init(&ipv4_parameters);

        ipv4_inst = amxd_object_findf(ip_dm, "%s.IPv4Address.%s", ip_interface, ipv4_addr_alias);
        assert_non_null(ipv4_inst);

        assert_int_equal(amxd_object_get_params(ipv4_inst, &ipv4_parameters, amxd_dm_access_protected), 0);
        assert_string_equal(addressing_type, GET_CHAR(&ipv4_parameters, "AddressingType"));

        assert_true(GET_BOOL(&ip_parameters, "IPv4Enable"));

        amxc_var_clean(&ipv4_parameters);
    }

    if(ipv6) {
        assert_true(GET_BOOL(&ip_parameters, "IPv6Enable"));
    }

    free(_ll);
    amxc_var_clean(&ip_parameters);
}

static void assert_cellular_mode(const char* ip_interface, bool ipv4, bool ipv6, const char* cellular_interface) {
    amxc_var_t cellular_interface_parameters;
    amxc_var_t cellular_accesspoint_parameters;
    amxc_var_t logical_parameters;
    amxd_object_t* cellular_dm = amxd_dm_findf(test_get_dm(), "Cellular.");
    amxd_object_t* cellular_interface_inst = amxd_object_findf(cellular_dm, "Interface.%s", cellular_interface);
    amxd_object_t* cellular_accesspoint_inst = NULL;
    amxd_object_t* logical_inst = amxd_dm_findf(test_get_dm(), "Logical.Interface.wan.");
    amxc_string_t expected_ll_string;

    amxc_string_init(&expected_ll_string, 0);
    amxc_var_init(&cellular_interface_parameters);
    amxc_var_init(&cellular_accesspoint_parameters);
    amxc_var_init(&logical_parameters);

    amxc_string_setf(&expected_ll_string, "Device.IP.%s.", ip_interface);

    assert_non_null(cellular_interface_inst);
    assert_int_equal(amxd_object_get_params(cellular_interface_inst, &cellular_interface_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&cellular_interface_parameters, "LowerLayers"));
    assert_int_equal(1, GET_BOOL(&cellular_interface_parameters, "Enable"));

    cellular_accesspoint_inst = amxd_object_findf(cellular_dm, ".AccessPoint.[Interface=='Device.Cellular.Interface.%d.']", amxd_object_get_index(cellular_interface_inst));
    assert_non_null(cellular_accesspoint_inst);
    assert_int_equal(amxd_object_get_params(cellular_accesspoint_inst, &cellular_accesspoint_parameters, amxd_dm_access_protected), 0);
    if(ipv4 && ipv6) {
        assert_string_equal("ipv4v6", GET_CHAR(&cellular_accesspoint_parameters, "TEST_IPType"));
    } else if(ipv4) {
        assert_string_equal("ipv4", GET_CHAR(&cellular_accesspoint_parameters, "TEST_IPType"));
    } else if(ipv6) {
        assert_string_equal("ipv6", GET_CHAR(&cellular_accesspoint_parameters, "TEST_IPType"));
    }

    assert_non_null(logical_inst);
    assert_int_equal(amxd_object_get_params(logical_inst, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal(amxc_string_get(&expected_ll_string, 0), GET_CHAR(&logical_parameters, "LowerLayers"));

    if(ipv4) {
        assert_routing_ipv4forward(ip_interface, "3GPP-NAS");
    }
    amxc_string_shrink(&expected_ll_string, 1); // Remove final dot in Device.IP.Interface.x.
    assert_ip_dm(ip_interface, ipv4 ? "primary" : NULL, ipv6 ? "GUA_3GPP_NAS" : NULL, "3GPP-NAS", amxc_string_get(&expected_ll_string, 0), "");

    amxc_string_clean(&expected_ll_string);
    amxc_var_clean(&logical_parameters);
    amxc_var_clean(&cellular_accesspoint_parameters);
    amxc_var_clean(&cellular_interface_parameters);
}

static void assert_dhcp_mode(bool nd_enable, const char* ip_interface, bool ipv4, bool ipv6, const char* addressing_type, const char* wan_mode, const char* ipv6_delegate) {
    amxc_var_t wan_manager_parameters;
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* wan_manager_inst = NULL;
    amxc_string_t dhcp_client;

    amxc_var_init(&wan_manager_parameters);
    amxc_string_init(&dhcp_client, 0);

    if((is_dhcp_mode(wan_mode) && (strcmp(addressing_type, "DHCP") == 0))) {
        assert_ip_dm(ip_interface, ipv4 ? "primary" : NULL, ipv6 ? "GUA_RA" : NULL, addressing_type, "Device.Ethernet.Link.2", ipv6_delegate);
    } else if(is_ppp_mode(wan_mode) && (strcmp(addressing_type, "IPCP") == 0)) {
        assert_ip_dm(ip_interface, ipv4 ? "primary" : NULL, ipv6 ? "GUA_RA" : NULL, addressing_type, "Device.PPP.Interface.1", ipv6_delegate);
    }

    wan_manager_inst = amxd_object_findf(wan_manager_dm, "WAN.%s.Intf.1", wan_mode);
    assert_non_null(wan_manager_inst);

    assert_int_equal(amxd_object_get_params(wan_manager_inst, &wan_manager_parameters, amxd_dm_access_protected), 0);

    if(ipv4) {
        if(strcmp(addressing_type, "Static") != 0) {
            amxd_object_t* dhcp_dm = amxd_dm_findf(test_get_dm(), "DHCPv4Client.Client.");
            amxd_object_t* dhcp_inst = amxd_object_findf(dhcp_dm, "[Interface==\"Device.IP.%s.\"]", ip_interface);
            assert_non_null(dhcp_inst);
            amxc_string_setf(&dhcp_client, "Device.DHCPv4.Client.%d.", dhcp_inst->index);
            assert_string_equal(amxc_string_get(&dhcp_client, 0), GET_CHAR(&wan_manager_parameters, "DHCPv4Reference"));
            assert_routing_ipv4forward(ip_interface, "DHCPv4");
        } else {
            assert_string_equal("Device.DHCPv4.Client.1.", GET_CHAR(&wan_manager_parameters, "DHCPv4Reference"));
            assert_routing_ipv4forward(ip_interface, "Static");
        }
    }
    if(ipv6) {
        amxd_object_t* neighbordiscovery_inst = NULL;
        amxc_var_t neighbordiscovery_parameters;
        amxc_string_t neighbor_discovery_path;

        amxc_var_init(&neighbordiscovery_parameters);
        amxc_string_init(&neighbor_discovery_path, 0);

        if(strcmp(addressing_type, "Static") != 0) {
            amxd_object_t* dhcp_dm = amxd_dm_findf(test_get_dm(), "DHCPv6Client.Client.");
            amxd_object_t* dhcp_inst = amxd_object_findf(dhcp_dm, "[Interface==\"Device.IP.%s.\"]", ip_interface);
            assert_non_null(dhcp_inst);
            amxc_string_setf(&dhcp_client, "Device.DHCPv6.Client.%d.", dhcp_inst->index);
            assert_string_equal(amxc_string_get(&dhcp_client, 0), GET_CHAR(&wan_manager_parameters, "DHCPv6Reference"));
        } else {
            assert_string_equal("Device.DHCPv6.Client.1.", GET_CHAR(&wan_manager_parameters, "DHCPv6Reference"));
        }

        amxc_string_set(&neighbor_discovery_path, GET_CHAR(&wan_manager_parameters, "NeighborDiscoveryReference"));
        amxc_string_replace(&neighbor_discovery_path, "Device.", "", UINT32_MAX);
        neighbordiscovery_inst = amxd_dm_findf(test_get_dm(), amxc_string_get(&neighbor_discovery_path, 0));

        assert_int_equal(amxd_object_get_params(neighbordiscovery_inst, &neighbordiscovery_parameters, amxd_dm_access_protected), 0);
        assert_int_equal(nd_enable, GET_BOOL(&neighbordiscovery_parameters, "Enable"));
        if(str_empty(ipv6_delegate) && (strcmp(addressing_type, "IPCP") == 0)) {
            assert_true(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));
        } else {
            assert_false(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));
        }

        amxc_string_clean(&neighbor_discovery_path);
        amxc_var_clean(&neighbordiscovery_parameters);
    }

    amxc_string_clean(&dhcp_client);
    amxc_var_clean(&wan_manager_parameters);
}

static void assert_ppp_mode(const char* username, const char* password, int ip_version, bool ipv6_numbered, const char* ip_interface, const char* wan_mode) {
    amxc_var_t ppp_parameters;
    amxc_var_t neighbordiscovery_parameters;
    amxc_var_t wan_manager_parameters;
    amxc_string_t neighbor_discovery_path;
    amxd_object_t* ppp_dm = amxd_dm_findf(test_get_dm(), "PPP.");
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* ppp_inst = amxd_object_findf(ppp_dm, "Interface.1");
    amxd_object_t* neighbordiscovery_inst = NULL;
    amxd_object_t* wan_manager_inst = NULL;
    const char* ipv6addr_delegate = (ipv6_numbered || ip_version == 4) ? "" : "Device.IP.Interface.3.";
    amxc_var_init(&ppp_parameters);
    amxc_var_init(&neighbordiscovery_parameters);
    amxc_var_init(&wan_manager_parameters);
    amxc_string_init(&neighbor_discovery_path, 0);

    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);

    assert_ip_dm(ip_interface, ip_version == 4 ? "primary" : NULL, ip_version == 6 ? "GUA_RA" : NULL, "IPCP", "Device.PPP.Interface.1", ipv6addr_delegate);

    wan_manager_inst = amxd_object_findf(wan_manager_dm, "WAN.%s.Intf.1", wan_mode);
    assert_non_null(wan_manager_inst);
    assert_int_equal(amxd_object_get_params(wan_manager_inst, &wan_manager_parameters, amxd_dm_access_protected), 0);
    if(ip_version == 4) {
        assert_string_equal("Device.PPP.Interface.1", GET_CHAR(&wan_manager_parameters, "PPPv4Reference"));
        assert_routing_ipv4forward(ip_interface, "IPCP");
    } else if(ip_version == 6) {
        assert_string_equal("Device.PPP.Interface.1", GET_CHAR(&wan_manager_parameters, "PPPv6Reference"));
    }

    assert_nr_instances("PPP.Interface.", 1);
    assert_nr_active_objects("PPP.Interface", 1);
    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&ppp_parameters, "Enable"));
    if(ip_version == 4) {
        assert_true(GET_BOOL(&ppp_parameters, "IPCPEnable"));
    }
    if(ip_version == 6) {
        assert_true(GET_BOOL(&ppp_parameters, "IPv6CPEnable"));
    }
    assert_string_equal(username, GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal(password, GET_CHAR(&ppp_parameters, "Password"));

    amxc_string_set(&neighbor_discovery_path, GET_CHAR(&wan_manager_parameters, "NeighborDiscoveryReference"));
    amxc_string_replace(&neighbor_discovery_path, "Device.", "", UINT32_MAX);
    neighbordiscovery_inst = amxd_dm_findf(test_get_dm(), amxc_string_get(&neighbor_discovery_path, 0));
    assert_int_equal(amxd_object_get_params(neighbordiscovery_inst, &neighbordiscovery_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "Enable"));
    if(!ipv6_numbered || (ip_version == 4)) {
        assert_false(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));
    } else {
        assert_true(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));
    }

    amxc_string_clean(&neighbor_discovery_path);
    amxc_var_clean(&wan_manager_parameters);
    amxc_var_clean(&neighbordiscovery_parameters);
    amxc_var_clean(&ppp_parameters);
}

static bool set_ipv4_mode(const char* ip_mode, const char* intf_alias, amxd_status_t expected_status) {
    amxc_var_t args;
    amxc_var_t ret;
    bool rc = false;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    assert_non_null(ip_mode);
    assert_non_null(intf_alias);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "IPv4Mode", ip_mode);
    amxc_var_add_key(cstring_t, &args, "InterfaceAlias", intf_alias);
    assert_int_equal(amxd_object_invoke_function(wan_mode, "setIPv4Mode", &args, &ret), expected_status);
    rc = GET_BOOL(&ret, "status");

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return rc;
}

static bool set_ipv6_mode(const char* ip_mode, const char* intf_alias, amxd_status_t expected_status) {
    amxc_var_t args;
    amxc_var_t ret;
    bool rc = false;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    assert_non_null(ip_mode);
    assert_non_null(intf_alias);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "IPv6Mode", ip_mode);
    amxc_var_add_key(cstring_t, &args, "InterfaceAlias", intf_alias);
    assert_int_equal(amxd_object_invoke_function(wan_mode, "setIPv6Mode", &args, &ret), expected_status);
    rc = GET_BOOL(&ret, "status");

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return rc;
}

static void reset_wan_mode() {
    amxc_var_t args;
    amxc_var_t ret;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxd_object_invoke_function(wan_mode, "Reset", &args, &ret);

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
}

void test_wan_manager_set_invalid_mode(UNUSED void** state) {
    amxc_var_t status;
    amxc_var_init(&status);

    assert_false(set_wan_mode("test", amxd_status_invalid_attr));
    assert_active_wan_mode("demo_ppp6mode", "Enabled");
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_dhcp_mode(true, "Interface.7", false, true, "DHCP", "demo_ppp6mode", "Device.IP.Interface.3.");
    assert_ppp_mode("softathome", "ppp6", 6, false, "Interface.7", "demo_ppp6mode");

    amxc_var_clean(&status);
}

void test_wan_manager_set_valid_mode(UNUSED void** state) {
    amxc_var_t status;

    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    amxc_var_clean(&status);
}

void test_wan_manager_set_multiple_valid_modes(UNUSED void** state) {
    extern bool check_iterator;
    check_iterator = true;
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    assert_true(set_wan_mode("demo_wanmode,demo_ppp6mode", amxd_status_ok));
    assert_ppp_mode("softathome", "ppp6", 6, false, "Interface.7", "demo_ppp6mode");
    assert_nr_active_objects("DHCPv6Client.Client", 2);
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_dhcp_mode(true, "Interface.7", false, true, "IPCP", "demo_ppp6mode", "Device.IP.Interface.3.");
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_nr_active_objects("PPP.Interface", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");
    check_iterator = false;
}

void test_wan_manager_set_multiple_invalid_modes(UNUSED void** state) {
    assert_false(set_wan_mode("demo_wanmode,demo_vlanmode", amxd_status_invalid_value));
    assert_active_wan_mode("demo_wanmode", "Enabled");
}

void test_wan_manager_set_multiple_valid_used_modes(UNUSED void** state) {
    assert_true(set_wan_mode("demo_vlanmode", amxd_status_ok));
    assert_dhcp_mode(true, "Interface.2", true, false, "DHCP", "demo_vlanmode", "");

    assert_true(set_wan_mode("demo_ppp6mode", amxd_status_ok));
    assert_ppp_mode("softathome", "ppp6", 6, false, "Interface.7", "demo_ppp6mode");
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_dhcp_mode(true, "Interface.7", false, true, "IPCP", "demo_ppp6mode", "Device.IP.Interface.3.");

    assert_true(enable_wan_mode("demo_wanmode", amxd_status_ok));
    assert_active_wan_mode("demo_ppp6mode,demo_wanmode", "Enabled");
    assert_ppp_mode("softathome", "ppp6", 6, false, "Interface.7", "demo_ppp6mode");
    assert_nr_active_objects("DHCPv6Client.Client", 2);
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");
    assert_dhcp_mode(true, "Interface.7", false, true, "IPCP", "demo_ppp6mode", "Device.IP.Interface.3.");

    assert_true(disable_wan_mode("demo_ppp6mode", amxd_status_ok));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    assert_true(set_wan_mode("demo_ppp6mode", amxd_status_ok));
    assert_ppp_mode("softathome", "ppp6", 6, false, "Interface.7", "demo_ppp6mode");
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_dhcp_mode(true, "Interface.7", false, true, "IPCP", "demo_ppp6mode", "Device.IP.Interface.3.");
}

void test_wan_manager_switch_to_invalid(UNUSED void** state) {
    amxc_var_t status;
    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    assert_false(set_wan_mode("test", amxd_status_invalid_attr));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    amxc_var_clean(&status);
}

void test_wan_manager_switch_to_valid_different_intf(UNUSED void** state) {
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    assert_true(set_wan_mode("demo_test", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_test", "");
}

void test_wan_manager_switch_to_valid_same_intf(UNUSED void** state) {
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    assert_true(set_wan_mode("demo_vlanmode", amxd_status_ok));
}

static void assert_obj_string(amxd_object_t* obj, const char* param_name, const char* value) {
    char* my_string = NULL;

    assert_non_null(obj);
    my_string = amxd_object_get_value(cstring_t, obj, param_name, NULL);
    assert_string_equal(my_string, value);
    free(my_string);
}

void test_wan_manager_default_route(UNUSED void** state) {
    amxd_object_t* routing_dm = amxd_dm_findf(test_get_dm(), "Routing.Router.1.");
    amxd_object_t* default_route_inst = NULL;

    assert_non_null(routing_dm);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.2.']");
    assert_non_null(default_route_inst);
    // Check that we haven't overriden the wrong default route instance
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    assert_obj_string(default_route_inst, "Origin", "DHCPv4");
    assert_obj_string(default_route_inst, "GatewayIPAddress", "");

    assert_true(set_wan_mode("demo_dslite", amxd_status_ok));
    assert_dslite();

    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.7.']");
    assert_non_null(default_route_inst);
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    assert_obj_string(default_route_inst, "Origin", "Static");
    assert_obj_string(default_route_inst, "GatewayIPAddress", "80.16.3.1");

    amxd_object_set_cstring_t(default_route_inst, "GatewayIPAddress", ""); // On target this is achieved through changing NetModel queries when changing wan modes
    test_handle_events();
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));

    assert_ppp_mode("softathome", "softathome", 4, false, "Interface.2", "demo_pppmode");
    assert_nr_active_objects("PPP.Interface.", 1);
    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.2.']");
    assert_non_null(default_route_inst);
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    assert_obj_string(default_route_inst, "Origin", "IPCP");
    assert_obj_string(default_route_inst, "GatewayIPAddress", "");

    assert_true(set_wan_mode("demo_staticmode", amxd_status_ok));

    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.2.']");
    assert_non_null(default_route_inst);
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    assert_obj_string(default_route_inst, "Origin", "Static");
    assert_obj_string(default_route_inst, "GatewayIPAddress", "80.16.3.1");
}

void test_wan_manager_set_static_ip(UNUSED void** state) {
    amxd_object_t* ip_dm = amxd_dm_findf(test_get_dm(), "IP.Interface.2.");
    amxd_object_t* ip_addr = NULL;

    assert_non_null(ip_dm);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");
    ip_addr = amxd_object_findf(ip_dm, "IPv4Address.[AddressingType == 'Static']");
    assert_true(ip_addr == NULL);

    assert_true(set_wan_mode("demo_staticmode", amxd_status_ok));
    ip_addr = amxd_object_findf(ip_dm, "IPv4Address.[AddressingType == 'Static' && IPAddress == '80.16.3.112']");
    assert_non_null(ip_addr);
    ip_addr = amxd_object_findf(ip_dm, "IPv6Address.[Origin == 'Static' && IPAddress == '2a02:1802:94:3200:10:18ff:fe01:cc01']");
    assert_non_null(ip_addr);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");
    ip_addr = amxd_object_findf(ip_dm, "IPv4Address.[AddressingType == 'Static']");
    assert_true(ip_addr == NULL);
}

void test_wan_manager_logical_interface(UNUSED void** state) {
    amxc_var_t value;
    amxd_object_t* logical_intf = amxd_dm_findf(test_get_dm(), "Logical.Interface.wan.");
    const char* logical_ll = NULL;

    amxc_var_init(&value);

    assert_non_null(logical_intf);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.", logical_ll);

    assert_true(set_wan_mode("demo_dslite", amxd_status_ok));
    assert_dslite();

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.7.,Device.IP.Interface.2.", logical_ll);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.", logical_ll);

    amxc_var_clean(&value);
}

void test_wan_manager_set_ppp_mode(UNUSED void** state) {
    amxc_var_t ppp_parameters;
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* ppp_dm = amxd_dm_findf(test_get_dm(), "PPP.");
    amxd_object_t* ppp_inst = amxd_object_findf(ppp_dm, "Interface.1");
    amxd_object_t* demo_pppmode_obj = NULL;
    amxc_var_init(&ppp_parameters);

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_false(GET_BOOL(&ppp_parameters, "Enable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("ppp6", GET_CHAR(&ppp_parameters, "Password"));

    /* Change wan_mode to demo_pppmode */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("ppp4", "softathome", 4, false, "Interface.2", "demo_pppmode");

    /* Change wan_mode to demo_ppp6mode */
    assert_true(set_wan_mode("demo_ppp6mode", amxd_status_ok));
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("softathome", "ppp6", 6, false, "Interface.7", "demo_ppp6mode");
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_dhcp_mode(true, "Interface.7", false, true, "IPCP", "demo_ppp6mode", "Device.IP.Interface.3.");

    /* Remove UserName for demo_pppmode */
    demo_pppmode_obj = amxd_object_findf(wan_manager_dm, "WAN.demo_pppmode.Intf.1");
    assert_non_null(demo_pppmode_obj);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "UserName", ""), 0);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "Password", "softathome"), 0);
    test_handle_events();

    /* Change wan_mode to demo_pppmode again */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));

    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("softathome", "softathome", 4, false, "Interface.2", "demo_pppmode");

    /* Remove Password for demo_ppp6mode */
    demo_pppmode_obj = amxd_object_findf(wan_manager_dm, "WAN.demo_ppp6mode.Intf.1");
    assert_non_null(demo_pppmode_obj);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "UserName", "softathome"), 0);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "Password", ""), 0);
    test_handle_events();

    /* Remove IPv6AddressDelegate from demo_ppp6mode (IPv6 numbered mode) */
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "IPv6AddressDelegate", ""), 0);
    test_handle_events();

    /* Change wan_mode to demo_ppp6mode again (IPv6 numbered mode)*/
    assert_true(set_wan_mode("demo_ppp6mode", amxd_status_ok));

    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("softathome", "softathome", 6, true, "Interface.7", "demo_ppp6mode");
    assert_nr_active_objects("DHCPv6Client.Client", 1);
    assert_dhcp_mode(true, "Interface.7", false, true, "IPCP", "demo_ppp6mode", "");

    amxc_var_clean(&ppp_parameters);
}

void test_wan_manager_set_link_mode(UNUSED void** state) {
    amxc_var_t logical_parameters;
    amxd_object_t* logical_dm = amxd_dm_findf(test_get_dm(), "Logical.");
    amxd_object_t* logical_inst_voip = amxd_object_findf(logical_dm, "Interface.2");
    amxd_object_t* logical_inst_mgmt = amxd_object_findf(logical_dm, "Interface.3");
    amxc_var_init(&logical_parameters);

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    assert_int_equal(amxd_object_get_params(logical_inst_voip, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&logical_parameters, "LowerLayers"));
    assert_int_equal(amxd_object_get_params(logical_inst_mgmt, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&logical_parameters, "LowerLayers"));

    /* Change wan_mode to demo_link */
    assert_true(set_wan_mode("demo_link", amxd_status_ok));

    assert_int_equal(amxd_object_get_params(logical_inst_voip, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("Device.IP.Interface.2.", GET_CHAR(&logical_parameters, "LowerLayers"));
    assert_int_equal(amxd_object_get_params(logical_inst_mgmt, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("Device.IP.Interface.2.", GET_CHAR(&logical_parameters, "LowerLayers"));

    amxc_var_clean(&logical_parameters);
}


void test_wan_manager_reset_ppp_mode(UNUSED void** state) {
    amxc_var_t ppp_parameters;
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* ppp_dm = amxd_dm_findf(test_get_dm(), "PPP.");
    amxd_object_t* ppp_inst = amxd_object_findf(ppp_dm, "Interface.1");
    amxd_object_t* demo_pppmode_obj = NULL;
    amxc_var_init(&ppp_parameters);

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_false(GET_BOOL(&ppp_parameters, "Enable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    /* Change wan_mode to demo_pppmode */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));

    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("softathome", "softathome", 4, false, "Interface.2", "demo_pppmode");

    /* Change UserName and Password for demo_pppmode */
    demo_pppmode_obj = amxd_object_findf(wan_manager_dm, "WAN.demo_pppmode.Intf.1");
    assert_non_null(demo_pppmode_obj);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "UserName", "changed_user"), 0);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "Password", "changed_pw"), 0);
    test_handle_events();

    /* PPP datamodel is not changed yet */
    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("softathome", "softathome", 4, false, "Interface.2", "demo_pppmode");

    /* Reset wan mode */
    reset_wan_mode();
    assert_active_wan_mode("demo_pppmode", "Enabled");

    assert_nr_active_objects("PPP.Interface.", 1);
    assert_ppp_mode("changed_user", "changed_pw", 4, false, "Interface.2", "demo_pppmode");

    amxc_var_clean(&ppp_parameters);
}

void test_wan_manager_set_intf_ipv4_static_mode(UNUSED void** state) {
    amxd_object_t* wan_mgr = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* wan_mode = amxd_object_findf(wan_mgr, "WAN.1.");
    amxd_object_t* intf = amxd_dm_findf(test_get_dm(), "WANManager.WAN.demo_wanmode.Intf.1.");
    const char* ip_mode_str = NULL;
    const char* dns_mode_str = NULL;

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    test_handle_events();

    assert_false(set_ipv4_mode("test", "wan", amxd_status_invalid_value));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    ip_mode_str = object_const_string(intf, "IPv4Mode");
    dns_mode_str = object_const_string(wan_mode, "DNSMode");
    assert_non_null(ip_mode_str);
    assert_string_equal("dhcp4", ip_mode_str);

    assert_non_null(dns_mode_str);
    assert_string_equal("Dynamic", dns_mode_str);

    test_handle_events();

    assert_false(set_ipv4_mode("static", "wan", amxd_status_ok));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "Static", "demo_wanmode", "");

    ip_mode_str = object_const_string(intf, "IPv4Mode");
    dns_mode_str = object_const_string(wan_mode, "DNSMode");
    assert_non_null(ip_mode_str);
    assert_string_equal("static", ip_mode_str);

    assert_non_null(dns_mode_str);
    assert_string_equal("Static", dns_mode_str);

    assert_false(set_ipv4_mode("dhcp4", "wan", amxd_status_ok));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");
}

void test_wan_manager_set_intf_ipv6_static_mode(UNUSED void** state) {
    amxd_object_t* wan_mgr = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* wan_mode = amxd_object_findf(wan_mgr, "WAN.1.");
    amxd_object_t* intf = amxd_dm_findf(test_get_dm(), "WANManager.WAN.demo_wanmode.Intf.1.");
    amxd_object_t* ip_static_prefix = amxd_dm_findf(test_get_dm(), "IP.Interface.lan.IPv6Prefix.GUA_STATIC");
    const char* ip_mode_str = NULL;
    const char* dns_mode_str = NULL;

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    test_handle_events();

    assert_false(set_ipv6_mode("test", "wan", amxd_status_invalid_value));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("PPP.Interface.", 0);
    assert_dhcp_mode(true, "Interface.2", true, true, "DHCP", "demo_wanmode", "");

    ip_mode_str = object_const_string(intf, "IPv6Mode");
    dns_mode_str = object_const_string(wan_mode, "IPv6DNSMode");
    assert_non_null(ip_mode_str);
    assert_string_equal("dhcp6", ip_mode_str);

    assert_non_null(dns_mode_str);
    assert_string_equal("Dynamic", dns_mode_str);

    test_handle_events();

    assert_non_null(ip_static_prefix);

    assert_false(GET_BOOL(amxd_object_get_param_value(ip_static_prefix, "Enable"), NULL));

    assert_false(set_ipv6_mode("static", "wan", amxd_status_ok));
    assert_active_wan_mode("demo_wanmode", "Enabled");

    ip_mode_str = object_const_string(intf, "IPv6Mode");
    dns_mode_str = object_const_string(wan_mode, "IPv6DNSMode");
    assert_non_null(ip_mode_str);
    assert_string_equal("static", ip_mode_str);

    assert_non_null(dns_mode_str);
    assert_string_equal("Static", dns_mode_str);

    test_handle_events();

    assert_true(GET_BOOL(amxd_object_get_param_value(ip_static_prefix, "Enable"), NULL));
}

void test_wan_manager_set_bridge_mode(UNUSED void** state) {
    amxd_object_t* obj = amxd_dm_findf(test_get_dm(), "Bridging.LastAddParameters.Test.");
    amxc_var_t params;

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("Bridge_mode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);

    assert_string_equal(GET_CHAR(&params, "Alias"), "INITIAL_VALUE");
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1");
    assert_string_equal(GET_CHAR(&params, "VlanName"), "INITIAL_VALUE");
    assert_true(GET_BOOL(&params, "Enable"));
    assert_int_equal(GET_UINT32(&params, "VlanId"), 999);
    assert_int_equal(GET_UINT32(&params, "VlanPriority"), 999);

    amxc_var_clean(&params);

    obj = amxd_dm_findf(test_get_dm(), "Bridging.LastDisableParameters.Test.");

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1");
    assert_int_equal(GET_UINT32(&params, "VlanId"), 999);

    amxc_var_clean(&params);
}

void test_wan_manager_set_bridge_vlanmode(UNUSED void** state) {
    amxd_object_t* obj = amxd_dm_findf(test_get_dm(), "Bridging.LastAddParameters.Test.");
    amxc_var_t params;

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("Bridge_vlanmode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);

    assert_string_equal(GET_CHAR(&params, "Alias"), "INITIAL_VALUE");
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1");
    assert_string_equal(GET_CHAR(&params, "VlanName"), "INITIAL_VALUE");
    assert_true(GET_BOOL(&params, "Enable"));
    assert_int_equal(GET_UINT32(&params, "VlanId"), 100);
    assert_int_equal(GET_UINT32(&params, "VlanPriority"), 3);

    amxc_var_clean(&params);

    obj = amxd_dm_findf(test_get_dm(), "Bridging.LastDisableParameters.Test.");

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1");
    assert_int_equal(GET_UINT32(&params, "VlanId"), 100);

    amxc_var_clean(&params);
}

void test_wan_manager_set_cellular_mode(UNUSED void** state) {
    amxd_object_t* wan_mode_obj = NULL;

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("Cellular.Interface.", 0);

    /* Change wan_mode to demo_cellular */
    assert_true(set_wan_mode("demo_cellular", amxd_status_ok));
    assert_active_wan_mode("demo_cellular", "Enabled");
    assert_cellular_mode("Interface.10", true, false, "CELLULAR0");
    assert_nr_active_objects("Cellular.Interface.", 1);

    /* Change wan_mode to demo_cellular_v4v6 */
    assert_true(set_wan_mode("demo_cellular_v4v6", amxd_status_ok));
    assert_active_wan_mode("demo_cellular_v4v6", "Enabled");
    assert_cellular_mode("Interface.10", true, true, "CELLULAR0");
    assert_nr_active_objects("Cellular.Interface", 1);

    /* Change wan_mode to demo_cellular */
    assert_true(set_wan_mode("demo_cellular", amxd_status_ok));
    assert_active_wan_mode("demo_cellular", "Enabled");
    assert_cellular_mode("Interface.10", true, false, "CELLULAR0");
    assert_nr_active_objects("Cellular.Interface", 1);

    /* Change wan_mode to demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    assert_active_wan_mode("demo_wanmode", "Enabled");
    assert_nr_active_objects("Cellular.Interface", 0);

    /* Change wan_mode to demo_cellular_v6 */
    assert_true(set_wan_mode("demo_cellular_v6", amxd_status_ok));
    assert_active_wan_mode("demo_cellular_v6", "Enabled");
    assert_cellular_mode("Interface.10", false, true, "CELLULAR0");
    assert_nr_active_objects("Cellular.Interface", 1);

    wan_mode_obj = amxd_dm_findf(test_get_dm(), "WANManager.WAN.demo_cellular_v6");
    assert_int_equal(amxd_object_set_value(bool, wan_mode_obj, "TEST_SkipDisableUpstreamIntf", true), 0);

    /* Change wan_mode to demo_pppmode */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));
    assert_active_wan_mode("demo_pppmode", "Enabled");
    assert_nr_active_objects("Cellular.Interface", 1);

    /* Change wan_mode to demo_cellular_v4v6 */
    assert_true(set_wan_mode("demo_cellular_v4v6", amxd_status_ok));
    assert_active_wan_mode("demo_cellular_v4v6", "Enabled");
    assert_cellular_mode("Interface.10", true, true, "CELLULAR0");
    assert_nr_active_objects("Cellular.Interface", 1);
}

