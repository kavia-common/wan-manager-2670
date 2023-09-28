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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_types.h>

#include "ctrl/mode_ctrl.h"
#include "dhcpc/dhcpc.h"
#include "ppp/ppp.h"
#include "staticc/static_controller.h"
#include "dslite/dslite.h"
#include "link/link.h"

#define ME "wan-man"

typedef struct {
    mode_ctrl_t type;
    mode_ctrl_t mode;
    ctrl_fn enable;
    ctrl_fn disable;
} controller_item_t;

controller_item_t controllers [] = {
    { TYPE_VLAN, IPv4_DHCP, dhcpc4_enable, dhcpc4_disable },
    { TYPE_UNTAGGED, IPv4_DHCP, dhcpc4_enable, dhcpc4_disable },
    { TYPE_VLAN, IPv6_DHCP, dhcpc6_enable, dhcpc6_disable },
    { TYPE_UNTAGGED, IPv6_DHCP, dhcpc6_enable, dhcpc6_disable },
    { TYPE_UNTAGGED, IPv4_DSLITE, dslite_enable, dslite_disable },
    { TYPE_VLAN, IPv4_DSLITE, dslite_enable, dslite_disable },
    { TYPE_VLAN, IPv4_PPP | IPv6_PPP, ppp_enable, ppp_disable },
    { TYPE_UNTAGGED, IPv4_PPP | IPv6_PPP, ppp_enable, ppp_disable },
    { TYPE_VLAN, IPv4_STATIC, static4_enable, static4_disable},
    { TYPE_UNTAGGED, IPv4_STATIC, static4_enable, static4_disable},
    { TYPE_VLAN, IPv6_STATIC, static6_enable, static6_disable},
    { TYPE_UNTAGGED, IPv6_STATIC, static6_enable, static6_disable},
    { TYPE_VLAN, IPv4_LINK | IPv6_LINK, link_enable, link_disable},
    { TYPE_UNTAGGED, IPv4_LINK | IPv6_LINK, link_enable, link_disable},
    { (mode_ctrl_t) (TYPE_UNTAGGED | TYPE_VLAN | TYPE_ATM), IP_None, NULL, NULL },
    // last item of array must be 0
    { (mode_ctrl_t) 0, (mode_ctrl_t) 0, NULL, NULL }
};

static amxd_status_t mode_ctrl_call_fnc(controller_item_t* ctrl,
                                        mode_ctrl_t mode,
                                        const amxc_var_t* const parameters,
                                        bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    ctrl_fn call_fnc = enable ? ctrl->enable : ctrl->disable;

    // if NULL, no action is needed -> OK
    when_null_status(call_fnc, exit, rc = amxd_status_ok);
    rc = call_fnc(mode, parameters);
exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t mode_ctrl_action(mode_ctrl_t mode,
                               const amxc_var_t* const parameters,
                               bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_ok;
    controller_item_t* ctrll = controllers;
    int type = mode & MASK_TYPE;
    int ipmode = mode & (MASK_IPv4 | MASK_IPv6);

    SAH_TRACEZ_INFO(ME, "Type 0x%X ipmode 0x%X", type, ipmode);

    when_false_trace((ipmode != 0), exit, INFO,
                     "Nothing to do, IPv4Mode & IPv6Mode are 'none'");

    rc = amxd_status_unknown_error;
    when_null_trace(parameters, exit, ERROR, "Missing parameters");
    when_false_trace((type != 0), exit, ERROR, "Type is 0");

    for(int cnt = 0; (ctrll->type != 0) && (ipmode != 0); ctrll++, cnt++) {
        SAH_TRACEZ_INFO(ME, "%d: Type 0x%X ipmode 0x%X", cnt, ctrll->type, ctrll->mode);
        if(type != (type & (int) ctrll->type)) {
            continue;
        }
        if((ipmode & ctrll->mode) != 0) {
            rc = mode_ctrl_call_fnc(ctrll, mode, parameters, enable);
            // Clear bits of IPv4Mode and / or IPv6Mode
            ipmode &= ~ctrll->mode;
        }
    }
exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}
