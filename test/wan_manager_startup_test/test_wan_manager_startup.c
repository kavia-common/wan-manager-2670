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

#include "test_wan_manager_startup.h"
#include "dm_wan-manager.h"
#include "test_utils.h"

static void test_wan_manager_set_operation_mode(const char* mode);

void test_wan_manager_change_wan_mode_intf_type(UNUSED void** state) {
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_mode = amxd_dm_findf(test_get_dm(), "%sWANManager.WAN.demo_SFP", prefix);
    amxd_trans_t transaction;

    assert_non_null(wan_mode);

    char* intf = amxd_object_get_value(cstring_t, wan_mode, "PhysicalType", NULL);
    assert_non_null(intf);
    assert_string_equal(intf, "SFP");
    free(intf);

    amxd_trans_init(&transaction);
    amxd_trans_select_object(&transaction, wan_mode);
    amxd_trans_set_value(cstring_t, &transaction, "PhysicalType", "Ethernet");
    amxd_trans_apply(&transaction, test_get_dm());

    test_handle_events();

    intf = amxd_object_get_value(cstring_t, wan_mode, "PhysicalType", NULL);
    assert_string_equal(intf, "Ethernet");
    free(intf);

    amxd_trans_clean(&transaction);
}

void test_wan_manager_automatic_mode_enable_autosensing_module(UNUSED void** state) {
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_manager = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);
    char* operation_mode = NULL;
    bool enable = false;

    assert_non_null(wan_manager);

    operation_mode = amxd_object_get_cstring_t(wan_manager, "OperationMode", NULL);
    assert_string_equal("Manual", operation_mode);
    free(operation_mode);

    test_clear_amxb_calls();
    test_wan_manager_set_operation_mode("Automatic");
    assert_true(test_set_autosensing_called(&enable));
    assert_true(enable);
    operation_mode = amxd_object_get_cstring_t(wan_manager, "OperationMode", NULL);
    assert_string_equal("Automatic", operation_mode);
    free(operation_mode);

    test_clear_amxb_calls();
    test_wan_manager_set_operation_mode("Manual");
    assert_true(test_set_autosensing_called(&enable));
    assert_false(enable);
    operation_mode = amxd_object_get_cstring_t(wan_manager, "OperationMode", NULL);
    assert_string_equal("Manual", operation_mode);
    free(operation_mode);
}

void test_getCurrentWANModeStatus(UNUSED void** state) {
    amxc_var_t ret;
    amxd_trans_t trans;

    amxc_var_init(&ret);
    assert_int_equal(0, _getCurrentWANModeStatus(NULL, NULL, NULL, &ret));
    assert_false(GETP_BOOL(&ret, "status"));
    amxc_var_clean(&ret);

    amxd_trans_init(&trans);
    amxd_trans_select_pathf(&trans, "Device.IP.Interface.2.IPv4Address");
    amxd_trans_add_inst(&trans, 1, NULL);
    assert_int_equal(amxd_trans_apply(&trans, test_get_dm()), amxd_status_ok);
    amxd_trans_clean(&trans);

    amxc_var_init(&ret);
    assert_int_equal(0, _getCurrentWANModeStatus(NULL, NULL, NULL, &ret));
    assert_false(GETP_BOOL(&ret, "status"));
    amxc_var_clean(&ret);

    amxd_trans_init(&trans);
    amxd_trans_select_pathf(&trans, "Device.IP.Interface.2.IPv4Address.1.");
    amxd_trans_set_value(cstring_t, &trans, "IPAddress", "192.168.1.1");
    amxd_trans_set_value(cstring_t, &trans, "SubnetMask", "255.255.255.0");
    amxd_trans_set_value(cstring_t, &trans, "Status", "Enabled");
    assert_int_equal(amxd_trans_apply(&trans, test_get_dm()), amxd_status_ok);
    amxd_trans_clean(&trans);

    amxc_var_init(&ret);
    assert_int_equal(0, _getCurrentWANModeStatus(NULL, NULL, NULL, &ret));
    assert_true(GETP_BOOL(&ret, "status"));
    amxc_var_clean(&ret);

}

static void test_wan_manager_set_operation_mode(const char* mode) {
    amxd_dm_t* dm = test_get_dm();
    amxd_trans_t transaction;
    const char* prefix = test_get_prefix();
    amxd_object_t* wan_manager = amxd_dm_findf(test_get_dm(), "%sWANManager.", prefix);

    assert_non_null(wan_manager);

    amxd_trans_init(&transaction);
    amxd_trans_select_object(&transaction, wan_manager);
    amxd_trans_set_value(cstring_t, &transaction, "OperationMode", mode);
    amxd_trans_apply(&transaction, dm);

    test_handle_events();

    amxd_trans_clean(&transaction);
}