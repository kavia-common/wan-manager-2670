/****************************************************************************
**
** Copyright (c) 2022 SoftAtHome
**
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

#include "mock_netmodel.h"

#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "netmodel/nm_query.h"
#include "dm_wan_mode.h"

bool isUp_result = false;

bool __wrap_netmodel_initialize(void) {
    return true;
}

void __wrap_netmodel_cleanup(void) {
    return;
}

netmodel_query_t* __wrap_netmodel_openQuery_getFirstParameter(const char* intf,
                                                              const char* subscriber,
                                                              const char* name,
                                                              UNUSED const char* flag,
                                                              const char* traverse,
                                                              amxp_slot_fn_t handler,
                                                              void* userdata) {
    amxc_var_t data;
    amxc_var_init(&data);

    assert_non_null(intf);
    assert_non_null(subscriber);
    assert_non_null(traverse);
    assert_non_null(handler);
    assert_non_null(name);
    assert_string_equal(subscriber, "wan-manager");
    assert_string_equal(name, "InterfacePath");

    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));

    if(strcmp(intf, "NetModel.Intf.ethIntf-ETH0.") == 0) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->index for "Ethernet" is 0 (index in array)
        assert_int_equal(info->index, 0);
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

    assert_non_null(intf);
    assert_non_null(subscriber);
    assert_non_null(flag);
    assert_non_null(traverse);
    assert_non_null(handler);
    assert_string_equal(subscriber, "wan-manager");
    assert_string_equal(intf, "NetModel.Intf.resolver.");

    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));

    amxc_var_set_type(&data, AMXC_VAR_ID_LIST);

    if(strcmp(flag, "eth_intf && upstream") == 0) {
        nm_query_ll_info_t* info = (nm_query_ll_info_t*) userdata;
        // info->index for "Ethernet" is 0 (index in array)
        assert_int_equal(info->index, 0);
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
        // info->index for "Bridge" is 1 (index in array)
        assert_int_equal(info->index, 1);
        // 1. call function with no data
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
    amxd_object_t* wanmode_obj_priv = (amxd_object_t*) userdata;
    amxd_object_t* wanmode_obj = get_current_wan_mode();
    amxd_object_t* intf_obj = amxd_object_findf(wanmode_obj, "Intf.1.");
    char* ip_reference = amxd_object_get_value(cstring_t, intf_obj, "IPv4Reference", NULL);

    assert_non_null(intf);
    assert_non_null(subscriber);
    assert_non_null(flag);
    assert_non_null(traverse);
    assert_non_null(handler);
    assert_non_null(wanmode_obj_priv);
    assert_non_null(wanmode_obj);
    assert_non_null(ip_reference);

    assert_string_equal(intf, ip_reference);
    assert_string_equal(subscriber, "wan-manager");
    assert_string_equal(flag, "ipv4-up");
    assert_string_equal(traverse, netmodel_traverse_this);
    assert_string_equal(wanmode_obj_priv->name, wanmode_obj->name);

    free(ip_reference);
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
    amxd_object_t* current_mode_obj = get_current_wan_mode();
    char* ip_reference = NULL;

    printf("%s, %s, %s\n", interface, flag, traverse);
    fflush(stdout);

    assert_non_null(current_mode_obj);
    assert_non_null(interface);
    assert_non_null(flag);
    assert_non_null(traverse);

    amxd_object_for_each(instance, it, amxd_object_findf(current_mode_obj, ".Intf.")) {
        amxd_object_t* interface_obj = amxc_container_of(it, amxd_object_t, it);
        ip_reference = amxd_object_get_value(cstring_t, interface_obj, "IPv4Reference", NULL);
        assert_non_null(ip_reference);
        assert_string_equal(interface, ip_reference);
        assert_string_equal(flag, "ipv4-up");
        assert_string_equal(traverse, "this");
        break;
    }

    free(ip_reference);
    return isUp_result;
}
