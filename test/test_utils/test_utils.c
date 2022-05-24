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

#include <amxc/amxc_variant.h>
#include <amxc/amxc_lqueue.h>
#include <amxp/amxp_signal.h>
#include <amxd/amxd_dm.h>
#include <amxc/amxc_rbuffer.h>
#include <amxc/amxc_astack.h>
#include <amxc/amxc_lstack.h>
#include <amxo/amxo.h>
#include <amxp/amxp_slot.h>
#include <amxb/amxb.h>
#include <amxb/amxb_be.h>
#include <amxb/amxb_register.h>

#include <amxb/amxb.h>

#include <amxo/amxo.h>
#include <amxo/amxo_save.h>

#include "dm_wan-manager.h"
#include "test_utils.h"
#include "integration/dhcpc/dhcpc.h"
#include "dummy_backend.h"

typedef struct {
    bool was_called;
    bool enable;
} component_enable_set_t;

typedef struct {
    component_enable_set_t autosensing;
} amxb_set_calls_t;

static amxd_dm_t dm;
static amxb_set_calls_t calls;
static amxo_parser_t parser;
static const char* odl_defs = "../test_utils/wan-manager_test.odl";
static const char* odl_ip_mock = "../mocks/mock_ip.odl";

static void test_get_vlan(amxc_var_t* ret);
static void test_get_dhcp_client_wan(amxc_var_t* ret);
static void test_vlan_add_instance(amxc_var_t* ret, uint32_t id);
static void test_get_ip_interface_ipv4(amxc_var_t* ret);

int test_wan_manager_setup(UNUSED void** state) {
    amxd_object_t* root_obj = NULL;
    amxp_signal_t* signal = NULL;
    amxb_bus_ctx_t* bus_ctx = NULL;


    assert_int_equal(amxd_dm_init(&dm), amxd_status_ok);
    assert_int_equal(amxo_parser_init(&parser), 0);
    assert_int_equal(test_register_dummy_be(), 0);

    assert_int_equal(amxp_signal_new(NULL, &signal, strsignal(SIGCHLD)), 0);

    root_obj = amxd_dm_get_root(&dm);
    assert_non_null(root_obj);

    assert_int_equal(amxo_resolver_ftab_add(&parser, "print_event", AMXO_FUNC(_print_event)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "setWANMode", AMXO_FUNC(_setWANMode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "getWANMode", AMXO_FUNC(_getWANMode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "getCurrentWANModeStatus", AMXO_FUNC(_getCurrentWANModeStatus)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "update_autosensing", AMXO_FUNC(_update_autosensing)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "wan_mode_added", AMXO_FUNC(_wan_mode_added)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "set_wan_mode", AMXO_FUNC(_set_wan_mode)), 0);
    assert_int_equal(amxo_resolver_ftab_add(&parser, "interface_already_configured", AMXO_FUNC(_interface_already_configured)), 0);

    assert_int_equal(amxo_parser_parse_file(&parser, odl_defs, root_obj), 0);
    assert_int_equal(amxo_parser_parse_file(&parser, odl_ip_mock, root_obj), 0);

    assert_int_equal(amxb_connect(&bus_ctx, "dummy:/tmp/dummy.sock"), 0);
    assert_int_equal(amxo_connection_add(&parser,
                                         amxb_get_fd(bus_ctx),
                                         connection_read,
                                         "dummy:/tmp/dummy.sock",
                                         AMXO_BUS,
                                         bus_ctx)
                     , 0);
    assert_int_equal(amxb_register(bus_ctx, &dm), 0);


    _wan_manager_main(0, &dm, &parser);

    test_handle_events();

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

const char* test_get_prefix(void) {
    amxc_var_t* setting = amxo_parser_get_config(&parser, "prefix_");
    return amxc_var_constcast(cstring_t, setting);
}

int __wrap_amxb_add(amxb_bus_ctx_t* const bus_ctx,
                    const char* object,
                    UNUSED uint32_t index,
                    UNUSED const char* name,
                    amxc_var_t* values,
                    amxc_var_t* ret,
                    UNUSED int timeout) {
    int rc = 1;

    when_str_empty(object, exit);
    when_null(ret, exit);
    when_null(bus_ctx, exit);
    rc = 0;
    amxc_var_set_type(ret, AMXC_VAR_ID_LIST);

    if(0 == strcmp(object, "Device.Ethernet.VLANTermination.")) {
        amxc_var_t* instance = amxc_var_add(amxc_htable_t, ret, NULL);
        uint32_t id = GETP_UINT32(values, "VLANID");
        test_vlan_add_instance(instance, id);
    } else {
        assert_true(false);
    }

exit:
    return rc;
}

int __wrap_amxb_get(amxb_bus_ctx_t* const bus_ctx,
                    const char* object,
                    UNUSED int32_t depth,
                    amxc_var_t* ret,
                    UNUSED int timeout) {
    int rc = 1;
    amxc_var_t* objects = NULL;

    assert_non_null(object);
    assert_non_null(ret);
    assert_non_null(bus_ctx);

    amxc_var_set_type(ret, AMXC_VAR_ID_LIST);
    objects = amxc_var_add(amxc_htable_t, ret, NULL);
    rc = 0;
    if(0 == strcmp(object, "DHCPv4.Client.[Interface=='Device.IP.Interface.2.'].")) {
        test_get_dhcp_client_wan(objects);
    } else if(0 == strcmp(object, "Device.Ethernet.VLANTermination.[VLANID==201" \
                          " && LowerLayers=='Device.Ethernet.Link.2.'].")) {
        test_get_vlan(objects);
    } else if(0 == strcmp(object, "Device.IP.Interface.2.IPv4Address.*")) {
        test_get_ip_interface_ipv4(objects);
    } else {
        printf("Error: Unexpected query [%s]\n", object);
        assert_non_null(NULL);
        rc = 1;
    }
    fflush(stdout);

    return rc;

}

int __wrap_amxb_call(amxb_bus_ctx_t* const bus_ctx,
                     const char* object,
                     const char* method,
                     amxc_var_t* args,
                     amxc_var_t* ret,
                     UNUSED int timeout) {
    static int test_ip_interface_ll = 0;
    int rc = 1;

    when_str_empty(object, exit);
    when_null(ret, exit);
    when_null(bus_ctx, exit);

    if(0 == strcmp(method, "_set")) {
        if(0 == strcmp("DHCPv4.Client.1.", object)) {
            amxc_var_set_type(ret, AMXC_VAR_ID_LIST);
            amxc_var_t* client = amxc_var_add(amxc_htable_t, ret, NULL);
            amxc_var_t* first = amxc_var_add_key(amxc_htable_t, client, "DHCPv4.Client.1.", NULL);
            amxc_var_add_key(cstring_t, first, "Alias", "wan");
            rc = 0;
        } else if(0 == strcmp("Device.IP.Interface.2.", object)) {
            rc = 0;
            amxc_var_t* parameters = GET_ARG(args, "parameters");
            assert_non_null(parameters);
            const char* lower_layer = GET_CHAR(parameters, "LowerLayers");
            if(test_ip_interface_ll == 0) {
                assert_string_equal(lower_layer, "Device.Ethernet.Link.2.");
            } else if(test_ip_interface_ll == 1) {
                assert_string_equal(lower_layer, "");
            } else if(test_ip_interface_ll == 2) {
                assert_string_equal(lower_layer, "Device.Ethernet.Link.2.");
            } else if(test_ip_interface_ll == 3) {
                assert_string_equal(lower_layer, "");
            } else if(test_ip_interface_ll == 4) {
                assert_string_equal(lower_layer, "Device.Ethernet.Link.2.");
            } else if(test_ip_interface_ll == 5) {
                assert_string_equal(lower_layer, "");
            } else if(test_ip_interface_ll == 6) {
                assert_string_equal(lower_layer, "Device.Ethernet.VLANTermination.1.");
            } else {
                printf("\ntest_ip_interface_ll %d, lower layer %s\n",
                       test_ip_interface_ll, lower_layer);
                fflush(stdout);
                assert_true(false);
            }
            test_ip_interface_ll++;

        } else if(0 == strcmp("X_PRPL-COM_WANAutoSensing.", object)) {
            rc = 0;
            calls.autosensing.was_called = true;
            calls.autosensing.enable = GETP_BOOL(args, "parameters.Enable");
        } else if(0 == strcmp("Device.Ethernet.VLANTermination.1.", object)) {
            rc = 0;
        } else {
            printf("%s set function called\n", object);
            fflush(stdout);
            amxc_var_dump(args, STDOUT_FILENO);
            fflush(stdout);
            assert_true(false);
        }
    } else {
        printf("Error: Unexpected call [%s]\n", method);
        amxc_var_dump(args, STDOUT_FILENO);
        fflush(stdout);
        rc = 1;
    }
exit:

    return rc;
}

void test_handle_events(void) {
    while(amxp_signal_read() == 0) {
    }
}

void test_clear_amxb_calls(void) {
    memset(&calls, 0, sizeof(amxb_set_calls_t));
}
bool test_set_autosensing_called(bool* enable) {
    if(NULL != enable) {
        *enable = calls.autosensing.enable;
    }

    return calls.autosensing.was_called;
}

/*
 *
 * [
 *   {
 *       index = 2,
 *       name = "vlan201",
 *       object = "Device.Ethernet.VLANTermination.vlan201.",
 *       parameters = {
 *           Alias = "vlan201"
 *           Name = "vlan201",
 *       },
 *       path = "Device.Ethernet.VLANTermination.2."
 *   }
 *]
 *
 */

static void test_vlan_add_instance(amxc_var_t* ret, uint32_t id) {
    amxc_string_t object;
    amxc_string_init(&object, 0);
    if(NULL != ret) {
        amxc_string_setf(&object, "name%d", id);
        amxc_var_add_key(cstring_t, ret, "path", "Device.Ethernet.VLANTermination.1.");
        amxc_var_add_key(cstring_t, ret, "name", amxc_string_get(&object, 0));
    }
    amxc_string_clean(&object);
}

static void test_get_ip_interface_ipv4(amxc_var_t* ret) {
    amxc_var_t* link = NULL;

    if(NULL != ret) {
        link = amxc_var_add_key(amxc_htable_t, ret, "IP.Interface.2.IPv4Address.", NULL);
        amxc_var_add_key(cstring_t, link, "IPAddress", "192.168.1.117");
    }
}

/*
 *
 *
 *[
 *  {
 *      Device.Ethernet.VLANTermination.1. = {
 *      Alias = "vlan201",
 *      }
 *  }
 *]
 */

static void test_get_vlan(amxc_var_t* ret) {
    amxc_var_t* client = NULL;
    if(NULL != ret) {
        client = amxc_var_add_key(amxc_htable_t, ret, "Device.Ethernet.VLANTermination.1.", NULL);
        amxc_var_add_key(cstring_t, client, "Alias", "vlan201");
    }
}

/*
 *
 *
 *[
 *  {
 *      DHCPv4.Client.1. = {
 *      Alias = "wan",
 *      }
 *  }
 *]
 */
static void test_get_dhcp_client_wan(amxc_var_t* ret) {
    amxc_var_t* client = NULL;
    if(NULL != ret) {
        client = amxc_var_add_key(amxc_htable_t, ret, "DHCPv4.Client.1.", NULL);
        amxc_var_add_key(cstring_t, client, "Alias", "wan");
    }
}

