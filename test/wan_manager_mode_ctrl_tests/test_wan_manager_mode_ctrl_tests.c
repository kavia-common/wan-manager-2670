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

#include "ctrl/mode_ctrl.h"
#include "test_wan_manager_mode_ctrl_tests.h"

static amxd_status_t return_code = amxd_status_ok;
static amxd_status_t test_enable(wan_mode_type_t mode, const amxc_var_t* const params);
static amxd_status_t test_disable(wan_mode_type_t mode, const amxc_var_t* const params);

static const mode_ctrl_actions_t test_controller = {
    .enable = test_enable,
    .disable = test_disable,
};

int test_mode_ctrl_setup(UNUSED void** state) {
    /* Drop default controllers */
    mode_ctrl_cleanup();

    mode_ctrl_init();
    assert_int_equal(amxd_status_ok, register_mode_controller(Untagged_DHCP, &test_controller));
    return 0;
}
int test_mode_ctrl_teardown(UNUSED void** state) {
    mode_ctrl_cleanup();
    return 0;
}

void test_mode_ctrl_register_invalid_mode(UNUSED void** state) {
    assert_int_not_equal(amxd_status_ok, register_mode_controller(Mode_Nr_, &test_controller));
    assert_int_not_equal(amxd_status_ok, register_mode_controller(-1, &test_controller));
}
void test_mode_ctrl_register_invalid_controller(UNUSED void** state) {
    mode_ctrl_actions_t invalid_ctrl = {
        .enable = NULL,
        .disable = NULL
    };

    assert_int_not_equal(amxd_status_ok, register_mode_controller(Untagged_PPP, NULL));
    assert_int_not_equal(amxd_status_ok, register_mode_controller(Untagged_PPP, &invalid_ctrl));

    invalid_ctrl.enable = test_enable;
    assert_int_not_equal(amxd_status_ok, register_mode_controller(Untagged_PPP, &invalid_ctrl));

    invalid_ctrl.enable = NULL;
    invalid_ctrl.disable = test_disable;
    assert_int_not_equal(amxd_status_ok, register_mode_controller(Untagged_PPP, &invalid_ctrl));
}

void test_mode_ctrl_register_valid_mode(UNUSED void** state) {
    mode_ctrl_actions_t valid_ctrl = {
        .enable = test_enable,
        .disable = test_disable
    };
    assert_int_equal(amxd_status_ok, register_mode_controller(Untagged_PPP, &valid_ctrl));
}

void test_mode_ctrl_unregister_valid_mode(UNUSED void** state) {
    assert_int_equal(amxd_status_ok, unregister_mode_controller(Untagged_DHCP));
    assert_int_equal(amxd_status_ok, unregister_mode_controller(Untagged_PPP));
    assert_int_not_equal(amxd_status_ok, unregister_mode_controller(Untagged_DHCP));

}

void test_mode_ctrl_unregister_invalid_mode(UNUSED void** state) {
    assert_int_not_equal(amxd_status_ok, unregister_mode_controller(Mode_Nr_));
    assert_int_not_equal(amxd_status_ok, unregister_mode_controller(-1));
}

void test_mode_ctrl_call_valid_mode_enable(UNUSED void** state) {
    assert_int_equal(return_code, set_mode(Untagged_DHCP, NULL));
    return_code = amxd_status_function_not_implemented;
    assert_int_equal(return_code, set_mode(Untagged_DHCP, NULL));
    return_code = amxd_status_ok;
}

void test_mode_ctrl_call_valid_mode_disable(UNUSED void** state) {
    assert_int_equal(return_code, disable_mode(Untagged_DHCP, NULL));
    return_code = amxd_status_function_not_implemented;
    assert_int_equal(return_code, disable_mode(Untagged_DHCP, NULL));
    return_code = amxd_status_ok;
}

void test_mode_ctrl_call_invalid_mode_enable(UNUSED void** state) {
    assert_int_not_equal(return_code, set_mode(-1, NULL));
    assert_int_not_equal(return_code, set_mode(Mode_Nr_, NULL));
}

void test_mode_ctrl_call_invalid_mode_disable(UNUSED void** state) {
    assert_int_not_equal(return_code, disable_mode(-1, NULL));
    assert_int_not_equal(return_code, disable_mode(Mode_Nr_, NULL));
}

void test_mode_ctrl_call_unregister_mode_enable(UNUSED void** state) {
    assert_int_equal(amxd_status_function_not_implemented, set_mode(Untagged_PPP, NULL));
    assert_int_equal(amxd_status_function_not_implemented, set_mode(Tagged_DHCP, NULL));
}

void test_mode_ctrl_call_unregister_mode_disable(UNUSED void** state) {
    assert_int_equal(amxd_status_function_not_implemented, disable_mode(Untagged_PPP, NULL));
    assert_int_equal(amxd_status_function_not_implemented, disable_mode(Tagged_DHCP, NULL));
}

static amxd_status_t test_enable(UNUSED wan_mode_type_t mode, UNUSED const amxc_var_t* const params) {
    return return_code;
}

static amxd_status_t test_disable(UNUSED wan_mode_type_t mode, UNUSED const amxc_var_t* const params) {
    return return_code;
}