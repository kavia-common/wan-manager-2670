/****************************************************************************
**
** Copyright (c) 2021 SoftAtHome
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

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>

#include <debug/sahtrace.h>

#include "utils.h"
#include "ctrl/mode_ctrl.h"

#define ACTION_ENABLE 0
#define ACTION_DISABLE 1

typedef struct {
    amxc_htable_t controllers;
    bool initialized;
} mode_ctrl_manager_t;

typedef struct {
    amxc_htable_it_t hit;
    mode_ctrl_actions_t actions;
} controller_item_t;

static const char* wan_mode_type_t_str[Mode_Nr_] = {
    [Untagged_DHCP] = "Untagged_DHCP",
    [Untagged_PPP] = "Untagged_PPP",
    [Tagged_DHCP] = "Tagged_DHCP",
    [Tagged_PPP] = "Tagged_PPP",
};

static mode_ctrl_manager_t manager;

static void controller_item_delete(const char* key, amxc_htable_it_t* it);
static amxd_status_t mode_ctrl_validate_ctrl(const mode_ctrl_actions_t* const actions);
static amxd_status_t mode_ctrl_is_valid_mode(int mode);
static amxd_status_t add_controller(wan_mode_type_t mode, const mode_ctrl_actions_t* const actions);
static amxd_status_t mode_ctrl_action(int mode, const amxc_var_t* const parameters, int action_type);

const char* wan_mode_type_to_str(wan_mode_type_t mode) {
    if(amxd_status_ok == mode_ctrl_is_valid_mode(mode)) {
        return wan_mode_type_t_str[mode];
    }
    return NULL;
}

void mode_ctrl_init(void) {
    amxc_htable_init(&manager.controllers, 0);
    manager.initialized = true;
}

void mode_ctrl_cleanup(void) {
    amxc_htable_clean(&manager.controllers, controller_item_delete);
}

amxd_status_t register_mode_controller(int mode, const mode_ctrl_actions_t* const actions) {
    amxd_status_t rc = amxd_status_unknown_error;

    if(!manager.initialized) {
        mode_ctrl_init();
    }

    when_failed_l((rc = mode_ctrl_is_valid_mode(mode)), exit, "%d is not a valid WAN mode type", mode);
    when_failed((rc = mode_ctrl_validate_ctrl(actions)), exit);
    rc = add_controller((wan_mode_type_t) mode, actions);

exit:
    return rc;
}

amxd_status_t unregister_mode_controller(int mode) {
    amxd_status_t rc = amxd_status_unknown_error;
    const char* mode_str = NULL;
    amxc_htable_it_t* it = NULL;

    when_failed_l((rc = mode_ctrl_is_valid_mode(mode)), exit, "%d is not a valid WAN mode type", mode);
    mode_str = wan_mode_type_to_str((wan_mode_type_t) mode);
    it = amxc_htable_get(&manager.controllers, mode_str);

    if(NULL != it) {
        rc = amxd_status_ok;
        amxc_htable_it_clean(it, controller_item_delete);
    } else {
        rc = amxd_status_invalid_action;
    }

exit:
    return rc;
}

amxd_status_t set_mode(int mode, const amxc_var_t* const parameters) {
    return mode_ctrl_action(mode, parameters, ACTION_ENABLE);
}

amxd_status_t disable_mode(int mode, const amxc_var_t* const parameters) {
    return mode_ctrl_action(mode, parameters, ACTION_DISABLE);
}

static amxd_status_t mode_ctrl_action(int mode, const amxc_var_t* const parameters, int action_type) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_htable_it_t* it = NULL;
    controller_item_t* ctrl_actions = NULL;
    when_failed_l((rc = mode_ctrl_is_valid_mode(mode)), exit, "%d is not valid WAN mode type", mode);
    it = amxc_htable_get(&manager.controllers, wan_mode_type_to_str((wan_mode_type_t) mode));
    rc = amxd_status_function_not_implemented;
    when_null_l(it, exit, "No controller register for WAN mode type %s", wan_mode_type_to_str((wan_mode_type_t) mode));
    ctrl_actions = amxc_container_of(it, controller_item_t, hit);
    if(NULL != ctrl_actions) {
        rc = action_type == ACTION_ENABLE ?
            ctrl_actions->actions.enable((wan_mode_type_t) mode, parameters) :
            ctrl_actions->actions.disable((wan_mode_type_t) mode, parameters);
    }

exit:
    return rc;
}

static void controller_item_delete(UNUSED const char* key, amxc_htable_it_t* it) {
    controller_item_t* item = amxc_container_of(it, controller_item_t, hit);
    free(item);
}

static amxd_status_t mode_ctrl_validate_ctrl(const mode_ctrl_actions_t* const actions) {
    amxd_status_t rc = amxd_status_invalid_arg;

    when_null(actions, exit);
    when_null(actions->disable, exit);
    when_null(actions->enable, exit);

    rc = amxd_status_ok;

exit:
    return rc;
}

static amxd_status_t mode_ctrl_is_valid_mode(int mode) {
    return Mode_Nr_ > mode && Untagged_DHCP <= mode ? amxd_status_ok : amxd_status_invalid_arg;
}

static amxd_status_t add_controller(wan_mode_type_t mode, const mode_ctrl_actions_t* const actions) {
    controller_item_t* item = (controller_item_t*) calloc(1, sizeof(controller_item_t));

    item->actions.enable = actions->enable;
    item->actions.disable = actions->disable;
    SAH_TRACEZ_INFO(ME, "Register controller for %s mode type", wan_mode_type_to_str(mode));
    if(0 != amxc_htable_insert(&manager.controllers, wan_mode_type_to_str(mode), &item->hit)) {
        goto error;
    }

    return amxd_status_ok;

error:
    free(item);
    return amxd_status_unknown_error;
}