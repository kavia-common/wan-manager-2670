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

static bool set_wan_mode(const char* mode_to_set, amxd_status_t expected_status) {
    amxc_var_t args;
    amxc_var_t ret;
    bool rc = false;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    assert_non_null(mode_to_set);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "WANMode", mode_to_set);
    assert_int_equal(amxd_object_invoke_function(wan_mode, "setWANMode", &args, &ret), expected_status);
    rc = GETP_BOOL(&ret, "status");

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return rc;
}

void test_wan_manager_set_invalid_mode(UNUSED void** state) {
    amxc_var_t status;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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
    This test assumes that no instance is created be default in the datamodel of the Routing manager.
    The code has to create an instance if none are found in the datamodel.
    This test verifies that it is actually the case
 */
void test_wan_manager_routing_interface_create(UNUSED void** state) {
    amxd_object_t* routing_inst = amxd_dm_findf(test_get_dm(), "Device.Routing.RouteInformation.InterfaceSetting.[Interface == 'Device.IP.Interface.2.']");

    assert_true(routing_inst != NULL);
}

void test_wan_manager_routing_interface_switch(UNUSED void** state) {
    amxc_var_t status;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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

    assert_int_equal(reset_counter, get_reset_counter());
    amxc_var_clean(&status);
}

void test_wan_manager_dns_inst_add(UNUSED void** state) {
    amxc_var_t status;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
    amxd_object_t* dns_dm = amxd_dm_findf(test_get_dm(), "Device.DNS.Relay.");
    amxd_object_t* dns_server = NULL;
    const char* wan_mode_str = NULL;
    int reset_counter = 0;

    assert_non_null(dns_dm);

    amxc_var_init(&status);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    dns_server = amxd_object_findf(dns_dm, "Forwarding.[DNSServer == '1.1.1.1']");
    assert_null(dns_server);

    assert_true(set_wan_mode("demo_vlanmode", amxd_status_ok));

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);
    assert_string_equal("demo_vlanmode", wan_mode_str);

    dns_server = amxd_object_findf(dns_dm, "Forwarding.[DNSServer == '1.1.1.1']");
    assert_non_null(dns_server);

    assert_int_equal(reset_counter, get_reset_counter());

    amxc_var_clean(&status);
}

void test_wan_manager_dns_inst_remove(UNUSED void** state) {
    amxc_var_t status;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
    amxd_object_t* dns_dm = amxd_dm_findf(test_get_dm(), "Device.DNS.Relay.");
    amxd_object_t* dns_server = NULL;
    const char* wan_mode_str = NULL;
    int reset_counter = 0;

    assert_non_null(dns_dm);

    amxc_var_init(&status);

    dns_server = amxd_object_findf(dns_dm, "Forwarding.[DNSServer == '1.1.1.1']");
    assert_non_null(dns_server);

    dns_server = amxd_object_findf(dns_dm, "Forwarding.[DNSServer == '2620:119:35::35']");
    assert_non_null(dns_server);

    assert_true(set_wan_mode("demo_wanmode", amxd_status_ok));
    reset_counter = get_reset_counter();

    amxd_object_get_param(wan_mode, "WANMode", &status);
    wan_mode_str = amxc_var_constcast(cstring_t, &status);

    assert_non_null(wan_mode_str);
    assert_string_equal("demo_wanmode", wan_mode_str);

    dns_server = amxd_object_findf(dns_dm, "Forwarding.[DNSServer == '1.1.1.1']");
    assert_null(dns_server);

    dns_server = amxd_object_findf(dns_dm, "Forwarding.[DNSServer == '2620:119:35::35']");
    assert_null(dns_server);

    assert_int_equal(reset_counter, get_reset_counter());

    amxc_var_clean(&status);
}

void test_wan_manager_logical_interface(UNUSED void** state) {
    amxc_var_t value;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mgr = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
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
