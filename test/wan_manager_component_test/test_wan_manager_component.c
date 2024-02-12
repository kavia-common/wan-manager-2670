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

#include "test_wan_manager_component.h"
#include "component.h"
#include "wan_manager_utils.h"
#include "test_utils.h"
#include "reset_mock.h"

void test_wan_manager_component_add_str_to_csv(UNUSED void** state) {
    amxc_var_t value;
    const char* logical = "Logical.Interface.wan.";
    amxd_object_t* logical_intf = amxd_dm_findf(test_get_dm(), logical);
    const char* logical_ll = NULL;

    assert_non_null(logical_intf);

    amxc_var_init(&value);

    assert_int_equal(component_set_str_param(logical, logical_get_context(), "LowerLayers", ""), amxd_status_ok);
    assert_int_equal(component_add_string_to_csv(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.2."), amxd_status_ok);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.", logical_ll);

    // Check that the same string added only once to the csv
    assert_int_equal(component_add_string_to_csv(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.2."), amxd_status_ok);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.", logical_ll);

    // Add second string to csv
    assert_int_equal(component_add_string_to_csv(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.6."), amxd_status_ok);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.2.,Device.IP.Interface.6.", logical_ll);

    amxc_var_clean(&value);
}

void test_wan_manager_component_remove_str_from_csv(UNUSED void** state) {
    amxc_var_t value;
    const char* logical = "Logical.Interface.wan.";
    amxd_object_t* logical_intf = amxd_dm_findf(test_get_dm(), logical);
    const char* logical_ll = NULL;

    assert_non_null(logical_intf);

    amxc_var_init(&value);

    assert_int_equal(component_set_str_param(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.2.,Device.IP.Interface.6.,Device.IP.Interface.2."), amxd_status_ok);
    assert_int_equal(component_remove_string_from_csv(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.2."), amxd_status_ok);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("Device.IP.Interface.6.", logical_ll);

    assert_int_equal(component_remove_string_from_csv(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.6."), amxd_status_ok);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("", logical_ll);

    assert_int_equal(component_remove_string_from_csv(logical, logical_get_context(), "LowerLayers", "Device.IP.Interface.6."), amxd_status_ok);

    amxd_object_get_param(logical_intf, "LowerLayers", &value);
    logical_ll = amxc_var_constcast(cstring_t, &value);

    assert_non_null(logical_ll);
    assert_string_equal("", logical_ll);

    amxc_var_clean(&value);
}

