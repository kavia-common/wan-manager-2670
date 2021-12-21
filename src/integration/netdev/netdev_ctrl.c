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
#include "dm_wan-manager.h"
#include "integration/netdev/netdev_ctrl.h"

typedef struct  {
    const char* object_path;
    const char* expression;
} netdev_subscription_t;

typedef struct {
    amxc_string_t interface;
    amxp_slot_fn_t cb_fn;
    amxb_subscription_t* subscription;
} netdev_ctr_t;

static netdev_ctr_t netdev_ctrl;
static const netdev_subscription_t subscription_data = {
    .object_path = "NetDev.Link",
    .expression = "notification == 'dm:object-changed'"
};

amxd_status_t netdev_ctrl_subscribe(const char* interface, amxp_slot_fn_t callback) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxc_string_init(&netdev_ctrl.interface, 0);

    when_null(interface, exit);
    when_null(callback, exit);

    netdev_ctrl.cb_fn = callback;
    amxc_string_set(&netdev_ctrl.interface, interface);
    rc = (AMXB_STATUS_OK == amxb_subscription_new(&netdev_ctrl.subscription,
                                                  wan_get_context(),
                                                  subscription_data.object_path,
                                                  subscription_data.expression,
                                                  callback, NULL)) ? amxd_status_ok : amxd_status_unknown_error;
    when_failed_l(rc,
                  exit,
                  "Cannot subscribe  for NetDev event: [path=%s,expression=%s]",
                  subscription_data.object_path,
                  subscription_data.expression);

exit:
    return rc;
}

amxd_status_t netdev_ctrl_unsubscribe(void) {
    amxd_status_t rc = (AMXB_STATUS_OK == amxb_subscription_delete(&netdev_ctrl.subscription)) ?
        amxd_status_ok : amxd_status_unknown_error;

    netdev_ctrl.cb_fn = NULL;
    netdev_ctrl.subscription = NULL;
    amxc_string_clean(&netdev_ctrl.interface);
    return rc;
}
