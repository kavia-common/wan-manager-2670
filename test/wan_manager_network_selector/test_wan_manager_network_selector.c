/**
 * @file test_wan_manager_network_selector.c
 *
 * @brief This file contains implementation of network_selector functionalities and
 *        event handling
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 AT&T
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * Subject to the terms and conditions of this license, each
 * copyright holder and contributor hereby grants to those receiving
 * rights under this license a perpetual, worldwide, non-exclusive,
 * no-charge, royalty-free, irrevocable (except for failure to
 * satisfy the conditions of this license) patent license to make,
 * have made, use, offer to sell, sell, import, and otherwise
 * transfer this software, where such license applies only to those
 * patent claims, already acquired or hereafter acquired, licensable
 * by such copyright holder or contributor that are necessarily
 * infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright
 * holders and non-copyrightable additions of contributors, in
 * source or binary form) alone; or
 *
 * (b) combination of their Contribution(s) with the work of
 * authorship to which such Contribution(s) was added by such
 * copyright holder or contributor, if, at the time the Contribution
 * is added, such addition causes such combination to be necessarily
 * infringed. The patent license shall not apply to any other
 * combinations which include the Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any
 * copyright holder or contributor is granted under this license,
 * whether expressly, by implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxm/amxm.h>

#include "dm_wan_mode.h"
#include "netmodel/nm_query.h"
#include "test_wan_manager_network_selector.h"
#include "test_utils.h"
#include "reset_mock.h"

#include "network-selector/network_selector.h"

void test_wan_manager_network_selector_start(UNUSED void** state) {
    char* operation_mode = NULL;
    amxd_dm_t* dm = test_get_dm();
    amxd_trans_t transaction;
    amxd_object_t* wan_manager = amxd_dm_findf(test_get_dm(), "WANManager.");

    assert_non_null(wan_manager);

    amxd_trans_init(&transaction);
    amxd_trans_select_object(&transaction, wan_manager);
    amxd_trans_set_value(cstring_t, &transaction, "OperationMode", "Automatic");
    amxd_trans_apply(&transaction, dm);

    test_handle_events();

    amxd_trans_clean(&transaction);

    operation_mode = amxd_object_get_cstring_t(wan_manager, "OperationMode", NULL);
    assert_string_equal("Automatic", operation_mode);
    free(operation_mode);

    _update_network_selector(NULL, NULL, NULL);
    assert_int_equal(network_selector_start(), 0);

    amxd_trans_init(&transaction);
    amxd_trans_select_object(&transaction, wan_manager);
    amxd_trans_set_value(cstring_t, &transaction, "OperationMode", "Manual");
    amxd_trans_apply(&transaction, dm);

    test_handle_events();

    amxd_trans_clean(&transaction);

    operation_mode = amxd_object_get_cstring_t(wan_manager, "OperationMode", NULL);
    assert_string_equal("Manual", operation_mode);
    free(operation_mode);

    _update_network_selector(NULL, NULL, NULL);
    assert_int_not_equal(network_selector_start(), 0);
}

void test_wan_manager_network_selector_set_wan_mode(UNUSED void** state) {
    amxc_var_t* data = NULL;
    amxc_var_t* ret = NULL;
    amxd_object_t* wanm_obj = get_wan_manager_obj();
    amxd_object_t* intf_obj = amxd_object_findf(wanm_obj, "WAN.test_mode.Intf.1.");
    char* wan_mode = NULL;

    amxc_var_new(&ret);

    wan_mode = amxd_object_get_value(cstring_t, wanm_obj, "WANMode", NULL);
    assert_non_null(wan_mode);
    assert_string_not_equal(wan_mode, "test_mode");
    free(wan_mode);

    amxc_var_delete(&data);
    data = read_json_from_file("test_data/test_mode.json");
    assert_int_equal(amxm_execute_function("self", MOD_DM_MNGR, "set-wan-mode", data, ret), 0);

    wan_mode = amxd_object_get_value(cstring_t, wanm_obj, "WANMode", NULL);
    assert_non_null(wan_mode);
    assert_string_equal(wan_mode, "test_mode");
    free(wan_mode);

    assert_non_null(intf_obj);
    assert_non_null(intf_obj->priv);

    amxc_var_delete(&data);
    amxc_var_delete(&ret);
}
