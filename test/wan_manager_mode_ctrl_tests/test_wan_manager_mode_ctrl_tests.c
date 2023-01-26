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

#define FNC_DHCPC_ENABLE    0x0001
#define FNC_DHCPC_DISABLE   0x0002
#define FNC_PPP_ENABLE      0x0004
#define FNC_PPP_DISABLE     0x0008

static int calls_flags = 0;

amxd_status_t __wrap_dhcpc4_enable(mode_ctrl_t mode,
                                   UNUSED const amxc_var_t* const parameters);
amxd_status_t __wrap_dhcpc4_disable(mode_ctrl_t mode,
                                    UNUSED const amxc_var_t* const parameters);
amxd_status_t __wrap_dhcpc6_enable(mode_ctrl_t mode,
                                   UNUSED const amxc_var_t* const parameters);
amxd_status_t __wrap_dhcpc6_disable(mode_ctrl_t mode,
                                    UNUSED const amxc_var_t* const parameters);
amxd_status_t __wrap_ppp_enable(mode_ctrl_t mode,
                                UNUSED const amxc_var_t* const parameters);
amxd_status_t __wrap_ppp_disable(mode_ctrl_t mode,
                                 UNUSED const amxc_var_t* const parameters);

amxd_status_t __wrap_dhcpc4_enable(UNUSED mode_ctrl_t mode,
                                   UNUSED const amxc_var_t* const parameters) {
    calls_flags |= FNC_DHCPC_ENABLE;
    return amxd_status_ok;
}

amxd_status_t __wrap_dhcpc4_disable(UNUSED mode_ctrl_t mode,
                                    UNUSED const amxc_var_t* const parameters) {
    calls_flags |= FNC_DHCPC_DISABLE;
    return amxd_status_ok;
}

amxd_status_t __wrap_dhcpc6_enable(UNUSED mode_ctrl_t mode,
                                   UNUSED const amxc_var_t* const parameters) {
    calls_flags |= FNC_DHCPC_ENABLE;
    return amxd_status_ok;
}

amxd_status_t __wrap_dhcpc6_disable(UNUSED mode_ctrl_t mode,
                                    UNUSED const amxc_var_t* const parameters) {
    calls_flags |= FNC_DHCPC_DISABLE;
    return amxd_status_ok;
}

amxd_status_t __wrap_ppp_enable(UNUSED mode_ctrl_t mode,
                                UNUSED const amxc_var_t* const parameters) {
    calls_flags |= FNC_PPP_ENABLE;
    return amxd_status_ok;
}

amxd_status_t __wrap_ppp_disable(UNUSED mode_ctrl_t mode,
                                 UNUSED const amxc_var_t* const parameters) {
    calls_flags |= FNC_PPP_DISABLE;
    return amxd_status_ok;
}

int test_mode_ctrl_setup(UNUSED void** state) {
    return 0;
}

int test_mode_ctrl_teardown(UNUSED void** state) {
    return 0;
}

void test_mode_ctrl_missing_parameter(UNUSED void** state) {
    assert_int_not_equal(mode_ctrl_action(TYPE_UNTAGGED | IPv4_DHCP, NULL, false), amxd_status_ok);
}

void test_mode_ctrl_invalid_mode(UNUSED void** state) {
    amxc_var_t parameters;

    amxc_var_init(&parameters);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "IPv4Reference", "Device.IP.Interface.2.");

    // Missing Type
    assert_int_not_equal(mode_ctrl_action(IPv4_DHCP, &parameters, false), amxd_status_ok);

    // Invalid Type
    assert_int_not_equal(mode_ctrl_action(TYPE_UNTAGGED | TYPE_VLAN | IPv4_DHCP, &parameters, false), amxd_status_ok);

    amxc_var_clean(&parameters);
}

static void test_mode_ctrl(mode_ctrl_t mode,
                           const amxc_var_t* const parameters,
                           bool enable,
                           amxd_status_t expected_status,
                           int expected_calls) {
    assert_int_equal(mode_ctrl_action(mode, parameters, enable), expected_status);
    assert_int_equal(calls_flags, expected_calls);
    calls_flags = 0;
}

void test_mode_ctrl_dhcpc4_enable_mode(UNUSED void** state) {
    amxc_var_t parameters;

    amxc_var_init(&parameters);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "IPv4Reference", "Device.IP.Interface.2.");

    // Type = "untagged", IPv4Mode = "dhcp4",  IPv6Mode = "none"
    test_mode_ctrl(TYPE_UNTAGGED | IPv4_DHCP, &parameters, true, amxd_status_ok, FNC_DHCPC_ENABLE);

    // Type = "untagged", IPv4Mode = "none", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_UNTAGGED | IPv6_DHCP, &parameters, true, amxd_status_ok, FNC_DHCPC_ENABLE);

    // Type = "untagged", IPv4Mode = "dhcp4", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_UNTAGGED | IPv4_DHCP | IPv6_DHCP, &parameters, true, amxd_status_ok, FNC_DHCPC_ENABLE);

    // Type = "vlan", IPv4Mode = "dhcp4",  IPv6Mode = "none",
    test_mode_ctrl(TYPE_VLAN | IPv4_DHCP, &parameters, true, amxd_status_ok, FNC_DHCPC_ENABLE);

    // Type = "vlan", IPv4Mode = "none", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_VLAN | IPv6_DHCP, &parameters, true, amxd_status_ok, FNC_DHCPC_ENABLE);

    // Type = "vlan", IPv4Mode = "dhcp4", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_VLAN | IPv4_DHCP | IPv6_DHCP, &parameters, true, amxd_status_ok, FNC_DHCPC_ENABLE);

    amxc_var_clean(&parameters);
}

void test_mode_ctrl_dhcpc4_disable_mode(UNUSED void** state) {
    amxc_var_t parameters;

    amxc_var_init(&parameters);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "IPv4Reference", "Device.IP.Interface.2.");

    // Type = "untagged", IPv4Mode = "dhcp4",  IPv6Mode = "none"
    test_mode_ctrl(TYPE_UNTAGGED | IPv4_DHCP, &parameters, false, amxd_status_ok, FNC_DHCPC_DISABLE);

    // Type = "untagged", IPv4Mode = "none", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_UNTAGGED | IPv6_DHCP, &parameters, false, amxd_status_ok, FNC_DHCPC_DISABLE);

    // Type = "untagged", IPv4Mode = "dhcp4", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_UNTAGGED | IPv4_DHCP | IPv6_DHCP, &parameters, false, amxd_status_ok, FNC_DHCPC_DISABLE);

    // Type = "vlan", IPv4Mode = "dhcp4",  IPv6Mode = "none",
    test_mode_ctrl(TYPE_VLAN | IPv4_DHCP, &parameters, false, amxd_status_ok, FNC_DHCPC_DISABLE);

    // Type = "vlan", IPv4Mode = "none", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_VLAN | IPv6_DHCP, &parameters, false, amxd_status_ok, FNC_DHCPC_DISABLE);

    // Type = "vlan", IPv4Mode = "dhcp4", IPv6Mode = "dhcp6"
    test_mode_ctrl(TYPE_VLAN | IPv4_DHCP | IPv6_DHCP, &parameters, false, amxd_status_ok, FNC_DHCPC_DISABLE);

    amxc_var_clean(&parameters);
}

void test_mode_ctrl_ppp_modes(UNUSED void** state) {
    amxc_var_t parameters;

    amxc_var_init(&parameters);
    amxc_var_set_type(&parameters, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &parameters, "IPv4Reference", "Device.IP.Interface.2.");

    // Type = "untagged", IPv4Mode = "ppp4",  IPv6Mode = "none"
    test_mode_ctrl(TYPE_UNTAGGED | IPv4_PPP, &parameters, true, amxd_status_ok, FNC_PPP_ENABLE);

    // Type = "vlan", IPv4Mode = "ppp4", IPv6Mode = "none"
    test_mode_ctrl(TYPE_VLAN | IPv4_PPP, &parameters, false, amxd_status_ok, FNC_PPP_DISABLE);

    // Type = "untagged", IPv4Mode = "none", IPv6Mode = "ppp6"
    test_mode_ctrl(TYPE_UNTAGGED | IPv6_PPP, &parameters, false, amxd_status_ok, FNC_PPP_DISABLE);

    // Type = "vlan", IPv4Mode = "none", IPv6Mode = "ppp6"
    test_mode_ctrl(TYPE_VLAN | IPv6_PPP, &parameters, true, amxd_status_ok, FNC_PPP_ENABLE);

    // Type = "untagged", IPv4Mode = "ppp4", IPv6Mode = "ppp6"
    test_mode_ctrl(TYPE_UNTAGGED | IPv4_PPP | IPv6_PPP, &parameters, false, amxd_status_ok, FNC_PPP_DISABLE);

    // Type = "vlan", IPv4Mode = "ppp4", IPv6Mode = "ppp6"
    test_mode_ctrl(TYPE_VLAN | IPv4_PPP | IPv6_PPP, &parameters, true, amxd_status_ok, FNC_PPP_ENABLE);

    amxc_var_clean(&parameters);
}