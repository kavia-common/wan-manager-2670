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

#include "dm_wan-manager.h"
#include "test_wan_manager_rpc.h"
#include "test_utils.h"
#include "reset_mock.h"

void test_get_wanmode(UNUSED void** state) {
    amxc_var_t ret;
    amxc_var_t* expected_data = NULL;
    amxd_object_t* wan_obj = amxd_dm_findf(test_get_dm(), "WANManager.");
    int result = 0;
    int rv = 0;

    amxc_var_init(&ret);

    _getWANMode(wan_obj, NULL, NULL, &ret);
    expected_data = read_json_from_file("test_data/test_getWANMode.json");
    rv = amxc_var_compare(&ret, expected_data, &result);
    assert_int_equal(rv, 0);
    assert_int_equal(result, 0);

    amxc_var_delete(&expected_data);
    amxc_var_clean(&ret);
}

void test_get_wanmode_new_mode(UNUSED void** state) {
    amxc_var_t ret;
    amxc_var_t* expected_data = NULL;
    amxd_object_t* wan_obj = amxd_dm_findf(test_get_dm(), "WANManager.");
    int result = 0;
    int rv = 0;

    amxc_var_init(&ret);

    assert_true(set_wan_mode("demo_dslite", amxd_status_ok));

    _getWANMode(wan_obj, NULL, NULL, &ret);
    expected_data = read_json_from_file("test_data/test_getWANMode_dslite.json");
    rv = amxc_var_compare(&ret, expected_data, &result);

    assert_int_equal(rv, 0);
    assert_int_equal(result, 0);

    amxc_var_delete(&expected_data);
    amxc_var_clean(&ret);
}

