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
                                                              UNUSED void* userdata) {
    amxc_var_t data;
    amxc_var_init(&data);

    assert_non_null(intf);
    assert_non_null(subscriber);
    assert_non_null(traverse);
    assert_non_null(handler);
    assert_non_null(name);
    assert_true(strcmp(subscriber, "wan-manager") == 0);
    assert_true(strcmp(intf, "NetModel.Intf.ethIntf-ETH0.") == 0);

    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));

    if(strcmp(name, "InterfacePath") == 0) {
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
        assert_string_equal(name, "unknown");
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
    assert_true(strcmp(subscriber, "wan-manager") == 0);
    assert_true(strcmp(intf, "NetModel.Intf.resolver.") == 0);

    netmodel_query_t* q = malloc(sizeof(netmodel_query_t*));


    amxc_var_set_type(&data, AMXC_VAR_ID_LIST);

    if(strcmp(flag, "eth_intf && upstream") == 0) {
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
    } else {
        assert_string_equal(flag, "unknown");
    }
    amxc_var_clean(&data);
    return q;
}

void __wrap_netmodel_closeQuery(netmodel_query_t* query) {
    free(query);
}


