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

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxm/amxm.h>

#include <debug/sahtrace.h>

#include "dm_wan_mode.h"
#include "test_wan_manager_autosensing.h"
#include "test_utils.h"
#include "reset_mock.h"

#include "autosensing/autosensing.h"

static const char* odl_mod_mock = "../mocks/mod_mock.odl";

/**
 * Tests the init for autosensing
 * Expectations:
 *      * autosensing_init should fail if no module path is set in the odl
 *      * If the module path is set, the function should work
 *      * After calling the init function the core function should be loaded
 */
void test_wan_manager_autosensing_init(UNUSED void** state) {
    amxd_object_t* root_obj = amxd_dm_get_root(test_get_dm());
    assert_non_null(root_obj);

    // If no mod path is loaded, the init should fail because no module can be found
    assert_int_equal(autosensing_init(), -1);

    // Set the mod path, now the init should work
    assert_int_equal(amxo_parser_parse_file(test_get_parser(), odl_mod_mock, root_obj), 0);
    assert_int_equal(autosensing_init(), 0);

    // Check if all local function are loaded
    assert_true(amxm_has_function("self", MOD_DM_MNGR, "set-mode"));
    assert_true(amxm_has_function("self", MOD_DM_MNGR, "isup-sensing-start"));
    assert_true(amxm_has_function("self", MOD_DM_MNGR, "isup-sensing-stop"));

}

/**
 * This test simulates the module setting the mode
 * Expectations:
 *      * Calling the set-mode function without an alias (mode) should fail
 *      * When set-mode is called with a valid name:
 *          * The function should be successful
 *          * The mode should be set in the datamodel
 *          * A query will be started and stored in the first interface object of the current wan-mode
 */
void test_wan_manager_autosensing_set_mode(UNUSED void** state) {
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
    data = read_json_from_file("test_data/test_mode_no_name.json");
    assert_int_equal(amxm_execute_function("self", MOD_DM_MNGR, "set-mode", data, ret), -1);

    amxc_var_delete(&data);
    data = read_json_from_file("test_data/test_mode.json");
    assert_int_equal(amxm_execute_function("self", MOD_DM_MNGR, "set-mode", data, ret), 0);

    wan_mode = amxd_object_get_value(cstring_t, wanm_obj, "WANMode", NULL);
    assert_non_null(wan_mode);
    assert_string_equal(wan_mode, "test_mode");
    free(wan_mode);

    assert_non_null(intf_obj);
    assert_non_null(intf_obj->priv);

    amxc_var_delete(&data);
    amxc_var_delete(&ret);
}

/**
 * This test call the isup-sensing-start and stop functions.
 * Expectations:
 *      * The priv should still be filed in from the previous test, calling start again should still work
 *      * After calling the stop the priv should be empty
 *      * After calling start again the priv should be filled in again
 */
void test_wan_manager_sensing_query(UNUSED void** state) {
    amxc_var_t* data = NULL;
    amxd_object_t* intf_obj = amxd_object_findf(get_current_wan_mode(), "Intf.1.");

    amxc_var_new(&data);

    assert_non_null(intf_obj);
    assert_non_null(intf_obj->priv);

    assert_int_equal(amxm_execute_function("self", MOD_DM_MNGR, "isup-sensing-start", data, data), 0);
    assert_non_null(intf_obj);
    assert_non_null(intf_obj->priv);

    assert_int_equal(amxm_execute_function("self", MOD_DM_MNGR, "isup-sensing-stop", data, data), 0);
    assert_non_null(intf_obj);
    assert_null(intf_obj->priv);

    assert_int_equal(amxm_execute_function("self", MOD_DM_MNGR, "isup-sensing-start", data, data), 0);
    assert_non_null(intf_obj);
    assert_non_null(intf_obj->priv);

    amxc_var_delete(&data);
}

void test_wan_manager_sensing_toggle(UNUSED void** state) {
    amxc_var_t* data = NULL;
    amxc_var_t* ret = NULL;

    amxc_var_new(&ret);

    data = read_json_from_file("test_data/test_empty.json");
    assert_int_equal(amxm_execute_function("mod-autosensing", MOD_AUTOSENSING_CTRL, "is-autosensing-running", data, ret), 0);
    assert_false(GET_BOOL(ret, "running"));
    assert_true(GET_BOOL(ret, "data_ok"));
    amxc_var_delete(&data);

    data = read_json_from_file("test_data/test_start_sensing.json");
    mod_autosensing_start();
    assert_int_equal(amxm_execute_function("mod-autosensing", MOD_AUTOSENSING_CTRL, "is-autosensing-running", data, ret), 0);
    assert_true(GET_BOOL(ret, "running"));
    assert_true(GET_BOOL(ret, "data_ok"));
    amxc_var_delete(&data);

    data = read_json_from_file("test_data/test_empty.json");
    mod_autosensing_stop();
    assert_int_equal(amxm_execute_function("mod-autosensing", MOD_AUTOSENSING_CTRL, "is-autosensing-running", data, ret), 0);
    assert_false(GET_BOOL(ret, "running"));
    assert_true(GET_BOOL(ret, "data_ok"));
    amxc_var_delete(&data);

    amxc_var_delete(&ret);
}