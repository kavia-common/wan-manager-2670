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
#include "reset_mock.h"

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
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    const char* wan_mode_str = NULL;

    amxc_var_init(&status);

    assert_false(set_wan_mode("test", amxd_status_invalid_attr));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    amxc_var_clean(&status);
}

void test_wan_manager_set_valid_mode(UNUSED void** state) {
    amxc_var_t status;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    const char* wan_mode_str = NULL;

    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    amxc_var_clean(&status);
}

void test_wan_manager_switch_to_invalid(UNUSED void** state) {
    amxc_var_t status;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    const char* wan_mode_str = NULL;

    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_false(set_wan_mode("test", amxd_status_invalid_attr));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);
    assert_string_equal("demo_wanmode", wan_mode_str);

    amxc_var_clean(&status);
}

void test_wan_manager_switch_to_valid_different_intf(UNUSED void** state) {
    amxc_var_t status;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    const char* wan_mode_str = NULL;
    int reset_counter = get_reset_counter();
    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_true(set_wan_mode("demo_test", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);
    assert_string_equal("demo_test", wan_mode_str);

    assert_int_not_equal(reset_counter, get_reset_counter);

    amxc_var_clean(&status);
}

void test_wan_manager_switch_to_valid_same_intf(UNUSED void** state) {
    amxc_var_t status;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    const char* wan_mode_str = NULL;
    int reset_counter = 0;
    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_true(set_wan_mode("demo_vlanmode", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);
    assert_string_equal("demo_vlanmode", wan_mode_str);

    assert_int_equal(reset_counter, get_reset_counter());

    amxc_var_clean(&status);
}

/*
    This test assumes that no instance is created by default in the datamodel of the Routing manager.
    The code has to create an instance if none are found.
    This test then verifies that it is actually the case.
 */
void test_wan_manager_routing_interface_create(UNUSED void** state) {
    amxd_object_t* routing_inst = amxd_dm_findf(test_get_dm(), "Device.Routing.RouteInformation.InterfaceSetting.[Interface == 'Device.IP.Interface.2.']");

    assert_true(routing_inst != NULL);
}

void test_wan_manager_routing_interface_switch(UNUSED void** state) {
    amxc_var_t status;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* routing_dm = amxd_dm_findf(test_get_dm(), "Device.Routing.RouteInformation.");
    amxd_object_t* routing_inst = NULL;
    const char* wan_mode_str = NULL;
    int reset_counter = 0;
    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    routing_inst = amxd_object_findf(routing_dm, "InterfaceSetting.[Interface == 'Device.IP.Interface.2.']");
    assert_non_null(routing_inst);

    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);
    assert_string_equal("demo_pppmode", wan_mode_str);

    routing_inst = amxd_object_findf(routing_dm, "InterfaceSetting.[Interface == 'Device.IP.Interface.2.']");
    assert_null(routing_inst);

    routing_inst = amxd_object_findf(routing_dm, "InterfaceSetting.[Interface == 'Device.IP.Interface.6.']");
    assert_non_null(routing_inst);

    assert_true(set_wan_mode("demo_vlanmode", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);
    assert_string_equal("demo_vlanmode", wan_mode_str);

    assert_int_equal(reset_counter, get_reset_counter());
    amxc_var_clean(&status);
}

void test_wan_manager_default_route(UNUSED void** state) {
    amxd_object_t* routing_dm = amxd_dm_findf(test_get_dm(), "Device.Routing.Router.1.");
    amxd_object_t* default_route_inst = NULL;
    char* origin = NULL;

    assert_non_null(routing_dm);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.2.']");
    assert_non_null(default_route_inst);
    // Check that we haven't overriden the wrong default route instance
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    origin = amxd_object_get_value(cstring_t, default_route_inst, "Origin", NULL);
    assert_string_equal(origin, "DHCPv4");
    free(origin);

    assert_true(set_wan_mode("demo_dslite", amxd_status_ok));

    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.7.']");
    assert_non_null(default_route_inst);
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    origin = amxd_object_get_value(cstring_t, default_route_inst, "Origin", NULL);
    assert_string_equal(origin, "Static");
    free(origin);

    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));

    default_route_inst = amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.2.']");
    assert_non_null(default_route_inst);
    assert_non_null(amxd_object_findf(routing_dm, "IPv4Forwarding.[Interface == 'Device.IP.Interface.12.']"));
    origin = amxd_object_get_value(cstring_t, default_route_inst, "Origin", NULL);
    assert_string_equal(origin, "IPCP");
    free(origin);

}

void test_wan_manager_set_static_ip(UNUSED void** state) {
    amxc_var_t status;
    const char* wan_mode_str = NULL;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* ip_dm = amxd_dm_findf(test_get_dm(), "Device.IP.Interface.2.");
    amxd_object_t* ip_addr = NULL;
    int reset_counter = 0;

    assert_non_null(ip_dm);

    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    ip_addr = amxd_object_findf(ip_dm, "IPv4Address.[AddressingType == 'Static']");
    assert_true(ip_addr == NULL);

    ip_addr = amxd_object_findf(ip_dm, "IPv6Address.[Origin == 'Static']");
    assert_true(ip_addr == NULL);

    assert_true(set_wan_mode("demo_staticmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_staticmode", wan_mode_str);

    ip_addr = amxd_object_findf(ip_dm, "IPv4Address.[AddressingType == 'Static' && IPAddress == '172.16.110.45']");
    assert_non_null(ip_addr);

    ip_addr = amxd_object_findf(ip_dm, "IPv6Address.[Origin == 'Static' && IPAddress == '2a02:1802:94:3200:10:18ff:fe01:cc01']");
    assert_non_null(ip_addr);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    ip_addr = amxd_object_findf(ip_dm, "IPv4Address.[AddressingType == 'Static']");
    assert_true(ip_addr == NULL);

    ip_addr = amxd_object_findf(ip_dm, "IPv6Address.[Origin == 'Static']");
    assert_true(ip_addr == NULL);

    assert_int_equal(reset_counter, get_reset_counter());

    amxc_var_clean(&status);
}

void test_wan_manager_logical_interface(UNUSED void** state) {
    amxc_var_t value;
    amxd_object_t* wan_mgr = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* logical_intf = amxd_dm_findf(test_get_dm(), "Device.Logical.Interface.wan.");
    const char* wan_mode_str = NULL;
    const char* logical_ll = NULL;

    amxc_var_init(&value);

    assert_non_null(logical_intf);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    amxd_object_get_param(wan_mgr, "WANMode", &value);
    wan_mode_str = amxc_var_constcast(cstring_t, &value);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.", logical_ll);

    assert_true(set_wan_mode("demo_dslite", amxd_status_ok));

    amxd_object_get_param(wan_mgr, "WANMode", &value);
    wan_mode_str = amxc_var_constcast(cstring_t, &value);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_dslite", wan_mode_str);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.,Device.IP.Interface.7.", logical_ll);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    amxd_object_get_param(wan_mgr, "WANMode", &value);
    wan_mode_str = amxc_var_constcast(cstring_t, &value);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.", logical_ll);

    amxc_var_clean(&value);
}

void test_wan_manager_set_ppp_mode(UNUSED void** state) {
    amxc_var_t wan_manager_parameters;
    amxc_var_t ppp_parameters;
    amxc_var_t ip_parameters;
    amxc_var_t neighbordiscovery_parameters;
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* ppp_dm = amxd_dm_findf(test_get_dm(), "Device.PPP.");
    amxd_object_t* ip_dm = amxd_dm_findf(test_get_dm(), "Device.IP.");
    amxd_object_t* neighbordiscovery_dm = amxd_dm_findf(test_get_dm(), "Device.NeighborDiscovery.");
    amxd_object_t* ppp_inst = amxd_object_findf(ppp_dm, "Interface.1");
    amxd_object_t* ip_inst = amxd_object_findf(ip_dm, "Interface.2");
    amxd_object_t* neighbordiscovery_inst = amxd_object_findf(neighbordiscovery_dm, "InterfaceSetting.1");
    amxd_object_t* demo_pppmode_obj = NULL;
    int reset_counter = 0;
    amxc_var_init(&wan_manager_parameters);
    amxc_var_init(&ppp_parameters);
    amxc_var_init(&ip_parameters);
    amxc_var_init(&neighbordiscovery_parameters);

    reset_counter = get_reset_counter();

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_wanmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_false(GET_BOOL(&ppp_parameters, "Enable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    /* Change wan_mode to demo_pppmode */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_pppmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&ppp_parameters, "Enable"));
    assert_true(GET_BOOL(&ppp_parameters, "IPCPEnable"));
    assert_false(GET_BOOL(&ppp_parameters, "IPv6CPEnable"));
    assert_string_equal("ppp4", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    assert_int_equal(amxd_object_get_params(ip_inst, &ip_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&ip_parameters, "IPv6AddressDelegate"));

    assert_int_equal(amxd_object_get_params(neighbordiscovery_inst, &neighbordiscovery_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "Enable"));
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));

    /* Change wan_mode to demo_ppp6mode */
    assert_true(set_wan_mode("demo_ppp6mode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_ppp6mode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&ppp_parameters, "Enable"));
    assert_false(GET_BOOL(&ppp_parameters, "IPCPEnable"));
    assert_true(GET_BOOL(&ppp_parameters, "IPv6CPEnable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("ppp6", GET_CHAR(&ppp_parameters, "Password"));

    assert_int_equal(amxd_object_get_params(ip_inst, &ip_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("Device.IP.Interface.3.", GET_CHAR(&ip_parameters, "IPv6AddressDelegate"));

    assert_int_equal(amxd_object_get_params(neighbordiscovery_inst, &neighbordiscovery_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "Enable"));
    assert_false(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));

    /* Remove UserName for demo_pppmode */
    demo_pppmode_obj = amxd_object_findf(wan_manager_dm, "WAN.demo_pppmode.Intf.1");
    assert_non_null(demo_pppmode_obj);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "UserName", ""), 0);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "Password", "softathome"), 0);
    test_handle_events();

    /* Change wan_mode to demo_pppmode again */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_pppmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&ppp_parameters, "Enable"));
    assert_true(GET_BOOL(&ppp_parameters, "IPCPEnable"));
    assert_false(GET_BOOL(&ppp_parameters, "IPv6CPEnable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    assert_int_equal(amxd_object_get_params(ip_inst, &ip_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&ip_parameters, "IPv6AddressDelegate"));

    assert_int_equal(amxd_object_get_params(neighbordiscovery_inst, &neighbordiscovery_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "Enable"));
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));

    /* Remove Password for demo_ppp6mode */
    demo_pppmode_obj = amxd_object_findf(wan_manager_dm, "WAN.demo_ppp6mode.Intf.1");
    assert_non_null(demo_pppmode_obj);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "UserName", "softathome"), 0);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "Password", ""), 0);
    test_handle_events();

    /* Change wan_mode to demo_ppp6mode again */
    assert_true(set_wan_mode("demo_ppp6mode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_ppp6mode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&ppp_parameters, "Enable"));
    assert_false(GET_BOOL(&ppp_parameters, "IPCPEnable"));
    assert_true(GET_BOOL(&ppp_parameters, "IPv6CPEnable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    assert_int_equal(amxd_object_get_params(ip_inst, &ip_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("Device.IP.Interface.3.", GET_CHAR(&ip_parameters, "IPv6AddressDelegate"));

    assert_int_equal(amxd_object_get_params(neighbordiscovery_inst, &neighbordiscovery_parameters, amxd_dm_access_protected), 0);
    assert_true(GET_BOOL(&neighbordiscovery_parameters, "Enable"));
    assert_false(GET_BOOL(&neighbordiscovery_parameters, "AutoConfEnable"));


    assert_int_equal(reset_counter, get_reset_counter());
    amxc_var_clean(&neighbordiscovery_parameters);
    amxc_var_clean(&ip_parameters);
    amxc_var_clean(&ppp_parameters);
    amxc_var_clean(&wan_manager_parameters);
}

void test_wan_manager_set_link_mode(UNUSED void** state) {
    amxc_var_t wan_manager_parameters;
    amxc_var_t logical_parameters;
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* logical_dm = amxd_dm_findf(test_get_dm(), "Device.Logical.");
    amxd_object_t* logical_inst_voip = amxd_object_findf(logical_dm, "Interface.2");
    amxd_object_t* logical_inst_mgmt = amxd_object_findf(logical_dm, "Interface.3");
    int reset_counter = 0;
    amxc_var_init(&wan_manager_parameters);
    amxc_var_init(&logical_parameters);

    reset_counter = get_reset_counter();

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_wanmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(logical_inst_voip, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&logical_parameters, "LowerLayers"));
    assert_int_equal(amxd_object_get_params(logical_inst_mgmt, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("", GET_CHAR(&logical_parameters, "LowerLayers"));

    /* Change wan_mode to demo_link */
    assert_true(set_wan_mode("demo_link", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_link", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(logical_inst_voip, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("Device.IP.Interface.2.", GET_CHAR(&logical_parameters, "LowerLayers"));
    assert_int_equal(amxd_object_get_params(logical_inst_mgmt, &logical_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("Device.IP.Interface.2.", GET_CHAR(&logical_parameters, "LowerLayers"));

    assert_int_equal(reset_counter, get_reset_counter());
    amxc_var_clean(&logical_parameters);
    amxc_var_clean(&wan_manager_parameters);
}


void test_wan_manager_reset_ppp_mode(UNUSED void** state) {
    amxc_var_t wan_manager_parameters;
    amxc_var_t ppp_parameters;
    amxd_object_t* wan_manager_dm = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* ppp_dm = amxd_dm_findf(test_get_dm(), "Device.PPP.");
    amxd_object_t* ppp_inst = amxd_object_findf(ppp_dm, "Interface.1");
    amxd_object_t* demo_pppmode_obj = NULL;
    int reset_counter = 0;
    amxc_var_init(&wan_manager_parameters);
    amxc_var_init(&ppp_parameters);

    reset_counter = get_reset_counter();

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_wanmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_false(GET_BOOL(&ppp_parameters, "Enable"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    /* Change wan_mode to demo_pppmode */
    assert_true(set_wan_mode("demo_pppmode", amxd_status_ok));
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_pppmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    /* Change UserName and Password for demo_pppmode */
    demo_pppmode_obj = amxd_object_findf(wan_manager_dm, "WAN.demo_pppmode.Intf.1");
    assert_non_null(demo_pppmode_obj);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "UserName", "changed_user"), 0);
    assert_int_equal(amxd_object_set_cstring_t(demo_pppmode_obj, "Password", "changed_pw"), 0);
    test_handle_events();

    /* PPP datamodel is not changed yet */
    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("softathome", GET_CHAR(&ppp_parameters, "Password"));

    /* Reset wan mode */
    reset_wan_mode();
    amxd_object_get_param(wan_manager_dm, "WANMode", &wan_manager_parameters);
    assert_string_equal("demo_pppmode", GET_CHAR(&wan_manager_parameters, NULL));

    assert_int_equal(amxd_object_get_params(ppp_inst, &ppp_parameters, amxd_dm_access_protected), 0);
    assert_string_equal("changed_user", GET_CHAR(&ppp_parameters, "Username"));
    assert_string_equal("changed_pw", GET_CHAR(&ppp_parameters, "Password"));

    assert_int_equal(reset_counter, get_reset_counter());
    amxc_var_clean(&ppp_parameters);
    amxc_var_clean(&wan_manager_parameters);
}

void test_wan_manager_set_intf_ipv4_static_mode(UNUSED void** state) {
    amxc_var_t status;
    amxc_var_t status_ip;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* intf = amxd_dm_findf(test_get_dm(), "WANManager.WAN.demo_wanmode.Intf.1.");
    const char* wan_mode_str = NULL;
    const char* ip_mode_str = NULL;

    amxc_var_init(&status);
    amxc_var_init(&status_ip);

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    test_handle_events();

    assert_false(set_ipv4_mode("test", "wan", amxd_status_invalid_value));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    amxd_object_get_param(intf, "IPv4Mode", &status_ip);
    ip_mode_str = amxc_var_constcast(cstring_t, &status_ip);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_non_null(ip_mode_str);
    assert_string_equal("dhcp4", ip_mode_str);

    test_handle_events();

    amxc_var_clean(&status);
    amxc_var_clean(&status_ip);

    amxc_var_init(&status);
    amxc_var_init(&status_ip);

    assert_false(set_ipv4_mode("static", "wan", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    amxd_object_get_param(intf, "IPv4Mode", &status_ip);
    ip_mode_str = amxc_var_constcast(cstring_t, &status_ip);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_non_null(ip_mode_str);
    assert_string_equal("static", ip_mode_str);

    amxc_var_clean(&status);
    amxc_var_clean(&status_ip);
}

void test_wan_manager_set_intf_ipv6_static_mode(UNUSED void** state) {
    amxc_var_t status;
    amxc_var_t status_ip;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");
    amxd_object_t* intf = amxd_dm_findf(test_get_dm(), "WANManager.WAN.demo_wanmode.Intf.1.");
    const char* wan_mode_str = NULL;
    const char* ip_mode_str = NULL;

    amxc_var_init(&status);
    amxc_var_init(&status_ip);

    /* Initial state: wan_mode = demo_wanmode */
    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));

    test_handle_events();

    assert_false(set_ipv6_mode("test", "wan", amxd_status_invalid_value));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    amxd_object_get_param(intf, "IPv6Mode", &status_ip);
    ip_mode_str = amxc_var_constcast(cstring_t, &status_ip);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_non_null(ip_mode_str);
    assert_string_equal("dhcp6", ip_mode_str);

    test_handle_events();

    amxc_var_clean(&status);
    amxc_var_clean(&status_ip);

    amxc_var_init(&status);
    amxc_var_init(&status_ip);

    assert_false(set_ipv6_mode("static", "wan", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    amxd_object_get_param(intf, "IPv6Mode", &status_ip);
    ip_mode_str = amxc_var_constcast(cstring_t, &status_ip);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    assert_non_null(ip_mode_str);
    assert_string_equal("static", ip_mode_str);

    amxc_var_clean(&status);
    amxc_var_clean(&status_ip);
}

void test_wan_manager_set_bridge_mode(UNUSED void** state) {
    amxd_object_t* obj = amxd_dm_findf(test_get_dm(), "Device.Bridging.LastAddParameters.Test.");
    amxc_var_t params;

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("Bridge_mode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);

    assert_string_equal(GET_CHAR(&params, "Alias"), "INITIAL_VALUE");
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1.");
    assert_string_equal(GET_CHAR(&params, "VlanName"), "INITIAL_VALUE");
    assert_true(GET_BOOL(&params, "Enable"));
    assert_int_equal(GET_UINT32(&params, "VlanId"), 999);
    assert_int_equal(GET_UINT32(&params, "VlanPriority"), 999);

    amxc_var_clean(&params);

    obj = amxd_dm_findf(test_get_dm(), "Device.Bridging.LastDisableParameters.Test.");

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1.");
    assert_int_equal(GET_UINT32(&params, "VlanId"), 999);

    amxc_var_clean(&params);
}

void test_wan_manager_set_bridge_vlanmode(UNUSED void** state) {
    amxd_object_t* obj = amxd_dm_findf(test_get_dm(), "Device.Bridging.LastAddParameters.Test.");
    amxc_var_t params;

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("Bridge_vlanmode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);

    assert_string_equal(GET_CHAR(&params, "Alias"), "INITIAL_VALUE");
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1.");
    assert_string_equal(GET_CHAR(&params, "VlanName"), "INITIAL_VALUE");
    assert_true(GET_BOOL(&params, "Enable"));
    assert_int_equal(GET_UINT32(&params, "VlanId"), 100);
    assert_int_equal(GET_UINT32(&params, "VlanPriority"), 3);

    amxc_var_clean(&params);

    obj = amxd_dm_findf(test_get_dm(), "Device.Bridging.LastDisableParameters.Test.");

    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    test_handle_events();

    amxd_object_get_params(obj, &params, amxd_dm_access_protected);
    assert_string_equal(GET_CHAR(&params, "LowerLayers"), "Device.Ethernet.Interface.1.");
    assert_int_equal(GET_UINT32(&params, "VlanId"), 100);

    amxc_var_clean(&params);
}