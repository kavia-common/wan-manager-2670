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
#include "dm_wan_mode_intf.h"
#include "ctrl/netdev_ctrl.h"
#include "ctrl/ipmanager_ctrl.h"

amxd_status_t wan_mode_intf_disable(amxd_object_t* const intf);
amxd_status_t wan_mode_intf_enable(amxd_object_t* const intf);

amxd_status_t wan_mode_intf_disable_all(amxd_object_t* const root) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* child = NULL;
    when_null(root, exit);

    child = amxd_object_get_child(root, "Intf");
    when_null(child, exit);

    rc = amxd_status_ok;
    amxd_object_for_each(instance, it, child) {
        amxd_object_t* obj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if(NULL != obj) {
            when_failed_l((rc = wan_mode_intf_disable(obj)), exit, "Disable Intf %s error", obj->name);
        }
    }

exit:
    return rc;
}

amxd_status_t wan_mode_intf_enable_all(amxd_object_t* const root) {
    amxd_status_t rc = amxd_status_unknown_error;
    amxd_object_t* child = NULL;
    when_null(root, exit);

    child = amxd_object_get_child(root, "Intf");
    when_null(child, exit);

    rc = amxd_status_ok;
    amxd_object_for_each(instance, it, child) {
        amxd_object_t* obj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if(NULL != obj) {
            when_failed_l((rc = wan_mode_intf_enable(obj)), exit, "Enable Intf %s error", obj->name);
        }
    }
exit:
    return rc;
}


amxd_status_t wan_mode_intf_disable(amxd_object_t* const intf) {
    amxd_status_t rc = amxd_status_unknown_error;
    when_null(intf, exit);

    when_failed_l((rc = ipmanager_ctrl_disable_intf(intf)), exit, "IPManager disable intf error %s", intf->name);
    when_failed_l((rc = netdev_ctrl_disable_intf(intf)), exit, "NetDev disable intf error %s", intf->name);

exit:
    return rc;
}
amxd_status_t wan_mode_intf_enable(amxd_object_t* const intf) {
    amxd_status_t rc = amxd_status_unknown_error;
    when_null(intf, exit);

    when_failed_l((rc = netdev_ctrl_disable_intf(intf)), exit, "NetDev configure intf error %s", intf->name);
    when_failed_l((rc = ipmanager_ctrl_disable_intf(intf)), exit, "IPManager configure intf error %s", intf->name);
    when_failed_l((rc = ipmanager_ctrl_configure_ipv4(intf)), exit, "IPManager IPv4 configure intf error %s", intf->name);
    when_failed_l((rc = ipmanager_ctrl_configure_ipv6(intf)), exit, "IPManager IPv6 configure intf error %s", intf->name);

exit:
    return rc;
}
