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
#include <errno.h>
#include <fcntl.h>
#include <yajl/yajl_gen.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxo/amxo.h>
#include <amxb/amxb.h>
#include <amxb/amxb_register.h>
#include <amxj/amxj_variant.h>
#include <amxd/amxd_transaction.h>

#include "ctrl/mode_ctrl.h"
#include "dm_wan-manager.h"
#include "dm_wan_mode.h"
#include "test_utils.h"
#include "dhcpc/dhcpc.h"
#include "dummy_backend.h"
#include "../mocks/mock_dns.h"
#include "../mocks/mock_netmodel.h"

typedef struct {
    bool was_called;
    bool enable;
} component_enable_set_t;

static amxd_dm_t dm;
static amxo_parser_t parser;
static const char* odl_defs = "../test_utils/wan-manager_test.odl";
static const char* odl_ip_mock = "../mocks/mock_ip.odl";
static const char* odl_routing_mock = "../mocks/mock_routing.odl";
static const char* odl_dns_mock = "../mocks/mock_dns.odl";
static const char* odl_ethernet_mock = "../mocks/mock_ethernet.odl";
static const char* odl_xpon_mock = "../mocks/mock_xpon.odl";
static const char* odl_logical_mock = "../mocks/mock_logical.odl";
static const char* odl_ppp_mock = "../mocks/mock_ppp.odl";
static const char* odl_neigbordiscovery_mock = "../mocks/mock_neighbordiscovery.odl";
static const char* odl_bridging_mock = "../mocks/mock_bridging.odl";
static const char* odl_dhcp_mock = "../mocks/mock_dhcp.odl";
static const char* odl_dslite_mock = "../mocks/mock_dslite.odl";
static const char* odl_pcp_mock = "../mocks/mock_pcp.odl";

static amxd_status_t _AddPort(UNUSED amxd_object_t* bridge_obj, UNUSED amxd_function_t* func, amxc_var_t* args, UNUSED amxc_var_t* ret) {
    amxd_status_t status = amxd_status_unknown_error;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    amxd_trans_select_pathf(&trans, "Bridging.LastAddParameters.Test.");
    amxd_trans_set_value(cstring_t, &trans, "LowerLayers", GET_CHAR(args, "LowerLayers"));
    if(GET_ARG(args, "Alias") != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "Alias", GET_CHAR(args, "Alias"));
    }
    amxd_trans_set_value(bool, &trans, "Enable", GET_BOOL(args, "Enable"));
    if(GET_ARG(args, "VlanId") != NULL) {
        amxd_trans_set_value(uint32_t, &trans, "VlanId", GET_UINT32(args, "VlanId"));
    }
    if(GET_ARG(args, "VlanName") != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "VlanName", GET_CHAR(args, "VlanName"));
    }
    if(GET_ARG(args, "VlanPriority") != NULL) {
        amxd_trans_set_value(uint32_t, &trans, "VlanPriority", GET_UINT32(args, "VlanPriority"));
    }

    status = amxd_trans_apply(&trans, &dm);
    amxd_trans_clean(&trans);
    return status;
}

static amxd_status_t _DisablePort(UNUSED amxd_object_t* bridge_obj, UNUSED amxd_function_t* func, amxc_var_t* args, UNUSED amxc_var_t* ret) {
    amxd_status_t status = amxd_status_unknown_error;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    amxd_trans_select_pathf(&trans, "Bridging.LastDisableParameters.Test.");
    amxd_trans_set_value(cstring_t, &trans, "LowerLayers", GET_CHAR(args, "LowerLayers"));
    if(GET_ARG(args, "VlanId") != NULL) {
        amxd_trans_set_value(uint32_t, &trans, "VlanId", GET_UINT32(args, "VlanId"));
    }

    status = amxd_trans_apply(&trans, &dm);

    amxd_trans_clean(&trans);
    return status;
}

static amxd_status_t _dummy(UNUSED amxd_object_t* object,
                            UNUSED amxd_param_t* param,
                            UNUSED amxd_action_t reason,
                            UNUSED const amxc_var_t* const args,
                            UNUSED amxc_var_t* const retval,
                            UNUSED void* priv) {
    return amxd_status_ok;
}

int test_wan_manager_setup(UNUSED void** state) {
    amxd_object_t* root_obj = NULL;
    amxp_signal_t* signal = NULL;
    amxb_bus_ctx_t* bus_ctx = NULL;
    amxd_object_t* wanm_obj = NULL;
    bool apply = false;

    assert_int_equal(amxd_dm_init(&dm), amxd_status_ok);
    assert_int_equal(amxo_parser_init(&parser), 0);
    assert_int_equal(test_register_dummy_be(), 0);

    assert_int_equal(amxp_signal_new(NULL, &signal, strsignal(SIGCHLD)), 0);

    root_obj = amxd_dm_get_root(&dm);
    assert_non_null(root_obj);

    assert_int_equal(amxo_resolver_ftab_add(&parser, "WANModeEnable", AMXO_FUNC(_WANModeEnable)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "WANModeDisable", AMXO_FUNC(_WANModeDisable)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "setWANMode", AMXO_FUNC(_setWANMode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "getWANMode", AMXO_FUNC(_getWANMode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "Reset", AMXO_FUNC(_Reset)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "update_autosensing", AMXO_FUNC(_update_autosensing)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "update_sensing_policy", AMXO_FUNC(_update_sensing_policy)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "wan_sensing_toggled", AMXO_FUNC(_wan_sensing_toggled)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "set_wan_mode", AMXO_FUNC(_set_wan_mode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "interface_already_configured", AMXO_FUNC(_interface_already_configured)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "check_wan_mode", AMXO_FUNC(_check_wan_mode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "dm_wan_manager_physical_type_changed", AMXO_FUNC(_dm_wan_manager_physical_type_changed)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "dm_wan_manager_wan_added", AMXO_FUNC(_dm_wan_manager_wan_added)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "wan_destroy", AMXO_FUNC(_wan_destroy)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "interface_destroy", AMXO_FUNC(_interface_destroy)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "ipv4_mode_toggled", AMXO_FUNC(_ipv4_mode_toggled)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "ipv6_mode_toggled", AMXO_FUNC(_ipv6_mode_toggled)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "setIPv4Mode", AMXO_FUNC(_setIPv4Mode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "setIPv6Mode", AMXO_FUNC(_setIPv6Mode)), 0);

    // Dummy functions
    assert_int_equal(amxo_resolver_ftab_add(&parser, "check_is_empty_or_in", AMXO_FUNC(_dummy)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "matches_regexp", AMXO_FUNC(_dummy)), 0);

    assert_int_equal(amxo_parser_parse_file(&parser, odl_defs, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_ip_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_routing_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_ethernet_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_xpon_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_logical_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_ppp_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_neigbordiscovery_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_dhcp_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_dslite_mock, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_pcp_mock, root_obj), 0);
    // Bridging rpc mocks
    assert_int_equal(amxo_resolver_ftab_add(&parser, "AddPort", AMXO_FUNC(_AddPort)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "DisablePort", AMXO_FUNC(_DisablePort)), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_bridging_mock, root_obj), 0);
    // DNS rpc mocks
    assert_int_equal(amxo_resolver_ftab_add(&parser, "SetForwarding", AMXO_FUNC(_setForwarding)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "DeleteForwarding", AMXO_FUNC(_deleteForwarding)), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_dns_mock, root_obj), 0);

    assert_int_equal(amxb_connect(&bus_ctx, "dummy:/tmp/dummy.sock"), 0);
    assert_int_equal(amxo_connection_add(&parser, amxb_get_fd(bus_ctx), connection_read,
                                         "dummy:/tmp/dummy.sock", AMXO_BUS, bus_ctx), 0);
    assert_int_equal(amxb_register(bus_ctx, &dm), 0);

    // Before startup, ApplyAtNextBoot should be true
    wanm_obj = amxd_dm_findf(&dm, "WANManager.");
    apply = amxd_object_get_value(bool, wanm_obj, "ApplyAtNextBoot", NULL);
    assert_true(apply);

    _wan_manager_main(0, &dm, &parser);
    // Do no expect netmodel_openQuery_getIntfs because PhysicalReference is set for demo_wanmode.
    expect_netmodel_openQuery_getFirstParameter("Device.Ethernet.Interface.1");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // demo_vlanmode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // test_mode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    expect_netmodel_openQuery_getIntfs("xpon && upstream");     // demo_pppmode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.xpon-cpe-EthernetUNI-1.");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // demo_staticmode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    expect_netmodel_openQuery_getIntfs("xpon && upstream");     // demo_ppp6mode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.xpon-cpe-EthernetUNI-1.");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // demo_dslite
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // demo_link
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // Bridge_mode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // Bridge_vlanmode
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    // SKIP demo_SFP since there is no physical flag defined
    expect_netmodel_openQuery_getIntfs("eth_intf && upstream"); // demo_test
    expect_netmodel_openQuery_getFirstParameter("NetModel.Intf.ethIntf-ETH0.");
    test_handle_events();

    // After startup ApplyAtNextBoot should be disabled
    apply = amxd_object_get_value(bool, wanm_obj, "ApplyAtNextBoot", NULL);
    assert_false(apply);

    return 0;
}

int test_wan_manager_teardown(UNUSED void** state) {
    _wan_manager_main(1, &dm, &parser);
    amxo_resolver_import_close_all();
    assert_int_equal(test_unregister_dummy_be(), 0);

    amxo_parser_clean(&parser);
    amxd_dm_clean(&dm);

    return 0;
}

amxd_dm_t* test_get_dm(void) {
    return &dm;
}

amxo_parser_t* test_get_parser(void) {
    return &parser;
}

void test_handle_events(void) {
    while(amxp_signal_read() == 0) {
    }
}

amxc_var_t* read_json_from_file(const char* fname) {
    int fd = -1;
    variant_json_t* reader = NULL;
    amxc_var_t* data = NULL;

    // create a json reader
    if(amxj_reader_new(&reader) != 0) {
        printf("Failed to create json file reader");
        goto exit;
    }

    // open the json file
    fd = open(fname, O_RDONLY);
    if(fd == -1) {
        printf("File open file %s - error 0x%8.8X\n", fname, errno);
        goto exit;
    }

    // read the json file and parse the json text
    while(amxj_read(reader, fd) > 0) {
    }

    // get the variant
    data = amxj_reader_result(reader);

    if(data == NULL) {
        printf("Invalid JSON in file %s\n", fname);
    }

    close(fd);
exit:
    amxj_reader_delete(&reader);
    return data;
}

bool enable_wan_mode(const char* mode_to_enable, amxd_status_t expected_status) {
    amxc_var_t args;
    amxc_var_t ret;
    bool rc = false;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    assert_non_null(mode_to_enable);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "WANMode", mode_to_enable);
    assert_int_equal(amxd_object_invoke_function(wan_mode, "WANModeEnable", &args, &ret), expected_status);
    rc = GET_BOOL(&ret, "status");

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return rc;
}

bool disable_wan_mode(const char* mode_to_disable, amxd_status_t expected_status) {
    amxc_var_t args;
    amxc_var_t ret;
    bool rc = false;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    assert_non_null(mode_to_disable);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "WANMode", mode_to_disable);
    assert_int_equal(amxd_object_invoke_function(wan_mode, "WANModeDisable", &args, &ret), expected_status);
    rc = GET_BOOL(&ret, "status");

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return rc;
}

bool set_wan_mode(const char* mode_to_set, amxd_status_t expected_status) {
    amxc_var_t args;
    amxc_var_t ret;
    bool rc = false;
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "WANManager.");

    amxc_var_init(&args);
    amxc_var_init(&ret);

    assert_non_null(wan_mode);
    assert_non_null(mode_to_set);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "WANMode", mode_to_set);
    assert_int_equal(amxd_object_invoke_function(wan_mode, "setWANMode", &args, &ret), expected_status);
    rc = GET_BOOL(&ret, "status");

    test_handle_events();

    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return rc;
}
