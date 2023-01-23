/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2021 SoftAtHome
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxm/amxm.h>

#include "mod-dummy.h"

char* test = NULL;
bool autosensing_running = false;

amxc_var_t* provided_sensing_data = NULL;

static int dummy_start_func(UNUSED const char* function_name,
                            amxc_var_t* args,
                            UNUSED amxc_var_t* ret) {
    amxc_var_delete(&provided_sensing_data);
    amxc_var_new(&provided_sensing_data);

    amxc_var_copy(provided_sensing_data, args);
    autosensing_running = true;
    return 0;
}

static int dummy_stop_func(UNUSED const char* function_name,
                           amxc_var_t* args,
                           UNUSED amxc_var_t* ret) {
    amxc_var_delete(&provided_sensing_data);
    amxc_var_new(&provided_sensing_data);

    amxc_var_copy(provided_sensing_data, args);
    autosensing_running = false;
    return 0;
}

static int is_autosensing_running(UNUSED const char* function_name,
                                  amxc_var_t* args,
                                  amxc_var_t* ret) {
    int rv = -1;
    int result = 0;

    amxc_var_set_type(ret, AMXC_VAR_ID_HTABLE);

    rv = amxc_var_compare(args, provided_sensing_data, &result);
    amxc_var_add_key(bool, ret, "data_ok", ((result == 0) && rv == 0));
    amxc_var_add_key(bool, ret, "running", autosensing_running);

    return 0;
}

static AMXM_CONSTRUCTOR dummy_module_start(void) {
    amxm_shared_object_t* so = amxm_so_get_current();
    amxm_module_t* mod = NULL;

    test = strdup("If stop is not called, valgrind will report this");
    (void) test;

    amxm_module_register(&mod, so, MOD_AUTOSENSING_CTRL);
    amxm_module_add_function(mod, "autosensing-start", dummy_start_func);
    amxm_module_add_function(mod, "autosensing-stop", dummy_stop_func);

    // Function for testing
    amxm_module_add_function(mod, "is-autosensing-running", is_autosensing_running);

    return 0;
}

static AMXM_DESTRUCTOR dummy_module_stop(void) {

    amxc_var_delete(&provided_sensing_data);

    free(test);
    return 0;
}
