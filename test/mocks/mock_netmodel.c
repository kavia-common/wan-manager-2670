/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice,
** this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** Subject to the terms and conditions of this license, each copyright holder
** and contributor hereby grants to those receiving rights under this license
** a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable
** (except for failure to satisfy the conditions of this license) patent license
** to make, have made, use, offer to sell, sell, import, and otherwise transfer
** this software, where such license applies only to those patent claims, already
** acquired or hereafter acquired, licensable by such copyright holder or contributor
** that are necessarily infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright holders and
** non-copyrightable additions of contributors, in source or binary form) alone;
** or
**
** (b) combination of their Contribution(s) with the work of authorship to which
** such Contribution(s) was added by such copyright holder or contributor, if,
** at the time the Contribution is added, such addition causes such combination
** to be necessarily infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any copyright
** holder or contributor is granted under this license, whether expressly, by
** implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
** USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include "mock_netmodel.h"

#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "netmodel/nm_query.h"
#include "dm_wan_mode.h"

bool isUp_result = false;

void expect_netmodel_openQuery_getFirstParameter(const char* interface) {
    expect_string(__wrap_netmodel_openQuery_getFirstParameter, intf, interface);
    expect_string(__wrap_netmodel_openQuery_getFirstParameter, subscriber, "wan-manager");
    expect_string(__wrap_netmodel_openQuery_getFirstParameter, name, "InterfacePath");
    expect_string(__wrap_netmodel_openQuery_getFirstParameter, flag, "");
    expect_string(__wrap_netmodel_openQuery_getFirstParameter, traverse, netmodel_traverse_one_level_up);
}

void expect_netmodel_openQuery_getIntfs(const char* flags) {
    expect_string(__wrap_netmodel_openQuery_getIntfs, intf, "NetModel.Intf.resolver.");
    expect_string(__wrap_netmodel_openQuery_getIntfs, subscriber, "wan-manager");
    expect_string(__wrap_netmodel_openQuery_getIntfs, flag, flags);
    expect_string(__wrap_netmodel_openQuery_getIntfs, traverse, "all");
}

bool __wrap_netmodel_initialize(void) {
    return true;
}

void __wrap_netmodel_cleanup(void) {
    return;
}

netmodel_query_t* __wrap_netmodel_openQuery_getFirstParameter(const char* intf,
                                                              const char* subscriber,
                                                              const char* name,
                                                              const char* flag,
                                                              const char* traverse,
                                                              amxp_slot_fn_t handler,
                                                              void* userdata) {
    amxc_var_t data;
    amxc_var_init(&data);

    check_expected(intf);
    check_expected(subscriber);
    check_expected(name);
    check_expected(flag);
    check_expected(traverse);

    assert_non_null(intf);
    assert_non_null(handler);
    assert_string_equal(subscriber, "wan-manager");
    assert_string_equal(name, "InterfacePath");

    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));

    if((strcmp(intf, "NetModel.Intf.ethIntf-ETH0.") == 0) || (strcmp(intf, "Device.Ethernet.Interface.1") == 0)) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->physical_type for "Ethernet" is 0 (index in physical_type_t)
        assert_int_equal(info->physical_type, 0);
        // 1. call function with no data
        handler("sig_name", &data, userdata);
        // 2. call function with empty string
        amxc_var_set(cstring_t, &data, "");
        handler("sig_name", &data, userdata);
        // 3. call with usefull data
        amxc_var_set(cstring_t, &data, "Device.Ethernet.Link.2.");
        handler("sig_name", &data, userdata);
        // 4. call with same data
        handler("sig_name", &data, userdata);
    } else if((strcmp(intf, "NetModel.Intf.xpon-cpe-EthernetUNI-1.") == 0) || (strcmp(intf, "Device.XPON.ONU.1.EthernetUNI.1.") == 0)) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->physical_type for "GPON" is 5 (index in physical_type_t)
        assert_int_equal(info->physical_type, 5);
        // 1. call function with no data
        handler("sig_name", &data, userdata);
        // 2. call function with empty string
        amxc_var_set(cstring_t, &data, "");
        handler("sig_name", &data, userdata);
        // 3. call with usefull data
        amxc_var_set(cstring_t, &data, "Device.Ethernet.Link.6.");
        handler("sig_name", &data, userdata);
        // 4. call with same data
        handler("sig_name", &data, userdata);
    } else if(strcmp(intf, "NetModel.Intf.bridge-eth_port2.") == 0) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->physical_type for "Bridge" is 1 (index in physical_type_t)
        assert_int_equal(info->physical_type, 1);
        amxc_var_set(cstring_t, &data, "Device.Bridging.Bridge.1.Port.2");
        handler("sig_name", &data, userdata);
    } else {
        assert_string_equal(name, "NetModel.Intf.unknown.");
    }
    amxc_var_clean(&data);
    return q;
}

netmodel_query_t* __wrap_netmodel_openQuery_getIntfs(const char* intf,
                                                     const char* subscriber,
                                                     const char* flag,
                                                     const char* traverse,
                                                     netmodel_callback_t handler,
                                                     void* userdata) {
    amxc_var_t data;
    amxc_var_init(&data);
    check_expected(intf);
    check_expected(subscriber);
    check_expected(flag);
    check_expected(traverse);

    assert_non_null(handler);

    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));

    amxc_var_set_type(&data, AMXC_VAR_ID_LIST);

    if(strcmp(flag, "eth_intf && upstream") == 0) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->physical_type for "Ethernet" is 0 (index in physical_type_t)
        assert_int_equal(info->physical_type, 0);
        // 1. call function with no data
        handler("sig_name", &data, userdata);
        // 2. call function with empty string
        //amxc_var_set(cstring_t, &data, "");
        //handler("sig_name", &data, userdata);
        // 3. call with usefull data
        amxc_var_add(cstring_t, &data, "ethIntf-ETH0");
        handler("sig_name", &data, userdata);
        // 4. call with same data
        handler("sig_name", &data, userdata);
    } else if(strcmp(flag, "bridge && upstream") == 0) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->physical_type for "Bridge" is 1 (index in physical_type_t)
        assert_int_equal(info->physical_type, 1);
        // 1. call function with no data
        handler("sig_name", &data, userdata);
        // 2. call with usefull data
        amxc_var_add(cstring_t, &data, "bridge-eth_port2");
        handler("sig_name", &data, userdata);
    } else if(strcmp(flag, "xpon && upstream") == 0) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->physical_type for "GPON" is 5 (index in physical_type_t)
        assert_int_equal(info->physical_type, 5);
        amxc_var_add(cstring_t, &data, "xpon-cpe-EthernetUNI-1");
        handler("sig_name", &data, userdata);
    } else {
        assert_string_equal(flag, "unknown");
    }
    amxc_var_clean(&data);
    return q;
}

netmodel_query_t* __wrap_netmodel_openQuery_isUp(const char* intf,
                                                 const char* subscriber,
                                                 const char* flag,
                                                 const char* traverse,
                                                 netmodel_callback_t handler,
                                                 UNUSED void* userdata) {
    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));
    amxd_object_t* priv_obj = NULL;
    const char* current_wanmodes = get_current_wan_mode_str();
    amxc_string_t current_wan_modes_str;
    amxc_llist_t current_list;
    amxd_object_t* intf_obj = NULL;

    amxc_string_init(&current_wan_modes_str, 0);
    amxc_llist_init(&current_list);

    if(!str_empty(flag)) {
        priv_obj = (amxd_object_t*) userdata;
        assert_non_null(priv_obj);
    }

    assert_non_null(intf);
    assert_non_null(subscriber);
    assert_non_null(flag);
    assert_non_null(traverse);
    assert_non_null(handler);
    assert_non_null(current_wanmodes);
    assert_string_equal(traverse, netmodel_traverse_this);
    assert_string_equal(subscriber, "wan-manager");

    amxc_string_set(&current_wan_modes_str, current_wanmodes);
    amxc_string_split_to_llist(&current_wan_modes_str, &current_list, ',');
    amxc_llist_for_each(it, &current_list) {
        const char* wan_mode = amxc_string_get(amxc_string_from_llist_it(it), 0);
        amxd_object_t* wan_mode_obj = get_wan_mode(wan_mode);
        char* ip_reference = NULL;
        assert_non_null(wan_mode_obj);

        // Check to see if interface object in userdata is an interface from the current wanmode
        if(!str_empty(flag)) {
            intf_obj = amxd_object_findf(wan_mode_obj, "Intf.%s.", priv_obj->name);
            if(intf_obj != priv_obj) {
                continue;
            }
            assert_non_null(intf_obj);
        }

        if(strcmp(flag, "ipv4-up") == 0) {
            ip_reference = amxd_object_get_value(cstring_t, intf_obj, "IPv4Reference", NULL);
            assert_non_null(ip_reference);
            assert_string_equal(intf, ip_reference);
            free(ip_reference);
        } else if(strcmp(flag, "ipv6-up") == 0) {
            ip_reference = amxd_object_get_value(cstring_t, intf_obj, "IPv6Reference", NULL);
            assert_non_null(ip_reference);
            assert_string_equal(intf, ip_reference);
            free(ip_reference);
        } else {
            assert_string_equal(flag, "");
        }


    }

    amxc_llist_clean(&current_list, amxc_string_list_it_free);
    amxc_string_clean(&current_wan_modes_str);
    return q;
}

void __wrap_netmodel_closeQuery(netmodel_query_t* query) {
    free(query);
}

void set_isUp_result(bool result) {
    isUp_result = result;
}

bool __wrap_netmodel_isUp(const char* const interface,
                          const char* const flag,
                          const char* const traverse) {
    const char* current_wanmodes = get_current_wan_mode_str();
    amxc_string_t current_wan_modes_str;
    amxc_llist_t current_list;

    amxc_string_init(&current_wan_modes_str, 0);
    amxc_llist_init(&current_list);

    assert_non_null(current_wanmodes);
    assert_non_null(interface);
    assert_non_null(flag);
    assert_non_null(traverse);

    amxc_string_set(&current_wan_modes_str, current_wanmodes);
    amxc_string_split_to_llist(&current_wan_modes_str, &current_list, ',');
    amxc_llist_for_each(it, &current_list) {
        char* ip_reference = NULL;
        const char* wan_mode = amxc_string_get(amxc_string_from_llist_it(it), 0);
        amxd_object_t* wan_mode_obj = get_wan_mode(wan_mode);
        assert_non_null(wan_mode_obj);

        amxd_object_for_each(instance, it, amxd_object_findf(wan_mode_obj, ".Intf.")) {
            amxd_object_t* interface_obj = amxc_container_of(it, amxd_object_t, it);
            ip_reference = amxd_object_get_value(cstring_t, interface_obj, "IPv4Reference", NULL);
            assert_non_null(ip_reference);
            assert_string_equal(interface, ip_reference);
            assert_string_equal(flag, "ipv4-up");
            assert_string_equal(traverse, "this");
            break;
        }

        free(ip_reference);
    }

    amxc_llist_clean(&current_list, amxc_string_list_it_free);
    amxc_string_clean(&current_wan_modes_str);
    return isUp_result;
}

amxc_var_t* __wrap_netmodel_getFirstParameter(const char* intf, const char* name, UNUSED const char* flag, const char* traverse) {
    amxc_var_t* data;
    amxc_var_new(&data);
    amxc_var_set_type(data, AMXC_VAR_ID_HTABLE);

    assert_non_null(intf);
    assert_non_null(name);
    assert_non_null(traverse);

    assert_string_equal(name, "InterfacePath");

    if(strcmp(intf, "NetModel.Intf.ethIntf-ETH0.") == 0) {
        assert_string_equal(traverse, netmodel_traverse_this);
        amxc_var_set(cstring_t, data, "Device.Ethernet.Interface.1");
    } else if(strcmp(intf, "NetModel.Intf.xpon-cpe-EthernetUNI-1.") == 0) {
        assert_string_equal(traverse, netmodel_traverse_this);
        amxc_var_set(cstring_t, data, "Device.XPON.ONU.1.EthernetUNI.1.");
    } else if(strcmp(intf, "NetModel.Intf.bridge-eth_port2.") == 0) {
        assert_string_equal(traverse, netmodel_traverse_this);
        amxc_var_set(cstring_t, data, "NetModel.Intf.bridge-eth_port2");
    } else if(strcmp(intf, "Bridging.Bridge.1.Port.1.") == 0) {
        assert_string_equal(traverse, netmodel_traverse_one_level_up);
        amxc_var_set(cstring_t, data, "Device.Ethernet.Link.3.");
    } else {
        // If this error is triggered, add a case to handle the calling interface
        assert_string_equal(intf, "unknown interface");
    }

    return data;
}


void __wrap_netmodel_clearFlag(const char* const intf, UNUSED const char* const flag, UNUSED const char* const condition, const char* const traverse) {
    assert_non_null(intf);
    assert_non_null(traverse);
    assert_string_equal(traverse, netmodel_traverse_this);
}
