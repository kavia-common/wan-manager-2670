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
#include "common_layers.h"

#define NR_OF_LAYERS 5
#define ME "wan-man"
typedef struct {
    mode_ctrl_t mode;
    ctrl_fn layers[NR_OF_LAYERS];
} controller_item_t;

controller_item_t controllers [] = {
    { IPv4_DHCP, {NULL, ipv4_layer, ip_enable, dhcp4_layer, logical_layer}},
    { IPv6_DHCP, {NULL, ipv6_layer, ip_enable, dhcp6_layer, logical_layer}},
    { IPv4_DSLITE, {NULL, NULL, NULL, dslite_layer, logical_layer}},
    { IPv4_PPP, {ppp_lower_layer, ipv4_layer, ip_enable, ppp_upper_layer, logical_layer}},
    { IPv6_PPP, {ppp_lower_layer, ipv6_layer, ip_enable, ppp_upper_layer, logical_layer}},
    { IPv4_STATIC, {NULL, ipv4_layer, ip_enable, static_layer, logical_layer}},
    { IPv6_STATIC, {NULL, ipv6_layer, ip_enable, static_layer, logical_layer}},
    { IPv4_LINK | IPv6_LINK, {NULL, NULL, NULL, NULL, logical_layer}},
    { IP_None, {NULL, NULL, NULL, NULL, NULL}},
    // last item of array must be 0
    { (mode_ctrl_t) 0, {NULL, NULL, NULL, NULL, NULL}}
};

static controller_item_t* get_mode_ctrl_action(const mode_ctrl_t mode, const ipversion_t ipversion) {
    SAH_TRACEZ_IN(ME);
    controller_item_t* ctrll = controllers;
    mode_ctrl_t ipmode = mode & (ipversion == IPv4 ? MASK_IPv4 : MASK_IPv6);

    when_false_status(ipversion_valid(ipversion), exit, ctrll = NULL);

    if(ipmode != 0) {
        for(int cnt = 0; (ipmode != 0); ctrll++, cnt++) {
            SAH_TRACEZ_INFO(ME, "%d: ipmode 0x%X", cnt, ctrll->mode);
            if((ipmode & ctrll->mode) != 0) {
                goto exit;
            }
        }
    } else {
        SAH_TRACEZ_INFO(ME, "Nothing to do, %s is 'none'", ipversion == IPv4 ? "IPv4Mode" : "IPv6Mode");
        // Make sure not to exit without setting the ctrll to NULL
    }

    ctrll = NULL;

exit:
    SAH_TRACEZ_OUT(ME);
    return ctrll;
}

amxd_status_t mode_ctrl_action(mode_ctrl_t mode,
                               amxc_var_t* const parameters,
                               bool enable) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rc = amxd_status_unknown_error;
    controller_item_t* ctrll_v4 = get_mode_ctrl_action(mode, IPv4);
    controller_item_t* ctrll_v6 = get_mode_ctrl_action(mode, IPv6);

    when_null_trace(parameters, exit, ERROR, "Missing parameters");

    if(enable) {
        int layer = 0;
        for(layer = 0; layer < NR_OF_LAYERS; layer++) {
            if((ctrll_v4 != NULL) && (ctrll_v4->layers[layer] != NULL)) {
                ctrll_v4->layers[layer](mode & MASK_IPv4, parameters, true);
            }
            if((ctrll_v6 != NULL) && (ctrll_v6->layers[layer] != NULL)) {
                ctrll_v6->layers[layer](mode & MASK_IPv6, parameters, true);
            }
        }
    } else {
        int layer = 0;
        for(layer = NR_OF_LAYERS - 1; layer >= 0; layer--) {
            if((ctrll_v4 != NULL) && (ctrll_v4->layers[layer] != NULL)) {
                ctrll_v4->layers[layer](mode & MASK_IPv4, parameters, false);
            }
            if((ctrll_v6 != NULL) && (ctrll_v6->layers[layer] != NULL)) {
                ctrll_v6->layers[layer](mode & MASK_IPv6, parameters, false);
            }
        }
    }

    rc = 0;

exit:
    SAH_TRACEZ_OUT(ME);
    return rc;
}
