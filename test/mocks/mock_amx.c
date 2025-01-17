/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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
#include <string.h>
#include <stdlib.h>

#include "mock_amx.h"

static char* remove_device_prefix(const char* str) {
    amxc_string_t object_path;
    char* res = NULL;
    amxc_string_init(&object_path, 0);
    amxc_string_setf(&object_path, "%s", str);
    amxc_string_replace(&object_path, "Device.DHCPv4.", "DHCPv4Client.", UINT32_MAX);
    amxc_string_replace(&object_path, "Device.DHCPv6.", "DHCPv6Client.", UINT32_MAX);
    if(amxc_string_search(&object_path, "Device.", 0) == 0) {
        amxc_string_replace(&object_path, "Device.", "", 1);
    }

    res = strdup(amxc_string_get(&object_path, 0));

    amxc_string_clean(&object_path);
    return res;
}

int __wrap_amxb_set(amxb_bus_ctx_t* const bus_ctx, const char* object, amxc_var_t* values, amxc_var_t* ret, int timeout) {
    int rv = -1;
    char* real_object = remove_device_prefix(object);

    rv = __real_amxb_set(bus_ctx, real_object, values, ret, timeout);
    free(real_object);

    return rv;
}


int __wrap_amxb_set_multiple(amxb_bus_ctx_t* const bus_ctx, uint32_t flags, amxc_var_t* req_paths, amxc_var_t* ret, int timeout) {
    int rv = -1;
    amxc_var_t* real_paths = NULL;

    amxc_var_new(&real_paths);
    amxc_var_copy(real_paths, req_paths);

    amxc_var_for_each(path, real_paths) {
        char* real_object = remove_device_prefix(GETP_CHAR(path, "parameters.ParentPrefix"));
        amxc_var_t real_path;

        amxc_var_init(&real_path);
        amxc_var_set(cstring_t, &real_path, real_object);
        amxc_var_set_path(path, "parameters.ParentPrefix", &real_path, AMXC_VAR_FLAG_DEFAULT);
        amxc_var_clean(&real_path);
        free(real_object);
    }

    rv = __real_amxb_set_multiple(bus_ctx, flags, real_paths, ret, timeout);

    amxc_var_delete(&real_paths);
    return rv;
}

int __wrap_amxb_get(amxb_bus_ctx_t* const bus_ctx, const char* object, int32_t depth, amxc_var_t* ret, int timeout) {
    int rv = -1;
    char* real_object = remove_device_prefix(object);

    rv = __real_amxb_get(bus_ctx, real_object, depth, ret, timeout);
    free(real_object);

    return rv;
}

int __wrap_amxb_add(amxb_bus_ctx_t* const bus_ctx, const char* object, uint32_t index, const char* name, amxc_var_t* values, amxc_var_t* ret, int timeout) {
    int rv = -1;
    char* real_object = remove_device_prefix(object);

    rv = __real_amxb_add(bus_ctx, real_object, index, name, values, ret, timeout);
    free(real_object);

    return rv;
}

int __wrap_amxb_call(amxb_bus_ctx_t* const bus_ctx, const char* object, const char* method, amxc_var_t* args, amxc_var_t* ret, int timeout) {
    int rv = -1;
    char* real_object = remove_device_prefix(object);

    rv = __real_amxb_call(bus_ctx, real_object, method, args, ret, timeout);
    free(real_object);

    return rv;
}

int __wrap_amxb_del(amxb_bus_ctx_t* const bus_ctx, const char* object, uint32_t index, const char* name, amxc_var_t* ret, int timeout) {
    int rv = -1;
    char* real_object = remove_device_prefix(object);

    rv = __real_amxb_del(bus_ctx, real_object, index, name, ret, timeout);
    free(real_object);

    return rv;
}
