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
#include "ctrl/ipmanager_ctrl.h"

typedef enum {
    IPv4_Mode_Dhcp,
    IPv4_Mode_Ppp,
    IPv4_Mode_None,
    IPv4_Mode_Static,
    IPv4_Mode_Invalid
} ipv4_mode_t;

typedef enum {
    IPv6_Mode_Dhcp,
    IPv6_Mode_Ppp,
    IPv6_Mode_None,
    IPv6_Mode_Static,
    IPv6_Mode_Invalid
} ipv6_mode_t;

static const char* ipv4_mode_str[IPv4_Mode_Invalid] = {
    "dhcp4",
    "ppp4",
    "none",
    "static"
};

static const char* ipv6_mode_str[IPv6_Mode_Invalid] = {
    "dhcp6",
    "ppp6",
    "none",
    "static"
};


static amxd_status_t ipmanager_ctrl_configure_ipv4_dhcp(amxd_object_t* interface);
static amxd_status_t ipmanager_ctrl_configure_ipv4_ppp(amxd_object_t* interface);
static amxd_status_t ipmanager_ctrl_configure_ipv4_static(amxd_object_t* interface);
static amxd_status_t ipmanager_ctrl_configure_ipv6_dhcp(amxd_object_t* interface);
static amxd_status_t ipmanager_ctrl_configure_ipv6_ppp(amxd_object_t* interface);
static amxd_status_t ipmanager_ctrl_configure_ipv6_static(amxd_object_t* interface);

static ipv4_mode_t ipv4_mode_from_str(const char* str);
static ipv6_mode_t ipv6_mode_from_str(const char* str);

amxd_status_t ipmanager_ctrl_subscribe(amxd_object_t* link) {
    return NULL != link ? amxd_status_ok : amxd_status_unknown_error;
}

amxd_status_t ipmanager_ctrl_unsubscribe(amxd_object_t* link) {
    return NULL != link ? amxd_status_ok : amxd_status_unknown_error;
}

amxd_status_t ipmanager_ctrl_disable_intf(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

amxd_status_t ipmanager_ctrl_configure_intf(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

amxd_status_t ipmanager_ctrl_configure_ipv4(amxd_object_t* interface) {
    amxd_status_t rc = amxd_status_invalid_value;
    amxc_var_t mode;
    const char* mode_str = NULL;
    when_null(interface, exit);

    when_failed(amxd_object_get_param(interface, "IPv4Mode", &mode), exit);
    mode_str = amxc_var_constcast(cstring_t, &mode);

    switch(ipv4_mode_from_str(mode_str)) {
    case IPv4_Mode_Dhcp: {
        rc = ipmanager_ctrl_configure_ipv4_dhcp(interface);
    } break;
    case IPv4_Mode_Ppp: {
        rc = ipmanager_ctrl_configure_ipv4_ppp(interface);
    }
    break;
    case IPv4_Mode_Static: {
        rc = ipmanager_ctrl_configure_ipv4_static(interface);
    } break;
    case IPv4_Mode_None: {
        rc = amxd_status_ok;
    }
    break;
    default:
        rc = amxd_status_unknown_error;
    }

exit:
    amxc_var_clean(&mode);
    return rc;
}

amxd_status_t ipmanager_ctrl_configure_ipv6(amxd_object_t* interface) {
    amxd_status_t rc = amxd_status_invalid_value;
    amxc_var_t mode;
    const char* mode_str = NULL;
    when_null(interface, exit);

    when_failed(amxd_object_get_param(interface, "IPv6Mode", &mode), exit);
    mode_str = amxc_var_constcast(cstring_t, &mode);

    switch(ipv6_mode_from_str(mode_str)) {
    case IPv6_Mode_Dhcp: {
        rc = ipmanager_ctrl_configure_ipv6_dhcp(interface);
    } break;
    case IPv6_Mode_Ppp: {
        rc = ipmanager_ctrl_configure_ipv6_ppp(interface);
    }
    break;
    case IPv6_Mode_Static: {
        rc = ipmanager_ctrl_configure_ipv6_static(interface);
    } break;
    case IPv6_Mode_None: {
        rc = amxd_status_ok;
    }
    break;
    default:
        rc = amxd_status_unknown_error;
    }

exit:
    amxc_var_clean(&mode);
    return rc;
}


static amxd_status_t ipmanager_ctrl_configure_ipv4_dhcp(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

static amxd_status_t ipmanager_ctrl_configure_ipv4_ppp(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

static amxd_status_t ipmanager_ctrl_configure_ipv4_static(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

static amxd_status_t ipmanager_ctrl_configure_ipv6_dhcp(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

static amxd_status_t ipmanager_ctrl_configure_ipv6_ppp(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

static amxd_status_t ipmanager_ctrl_configure_ipv6_static(amxd_object_t* interface) {
    return NULL != interface ? amxd_status_ok : amxd_status_unknown_error;
}

static ipv6_mode_t ipv6_mode_from_str(const char* str) {
    ipv6_mode_t mode = IPv6_Mode_Invalid;
    when_null(str, exit);
    for(int i = 0; i < IPv6_Mode_Invalid; ++i) {
        if(0 == strcmp(str, ipv6_mode_str[i])) {
            mode = (ipv6_mode_t) i;
        }
    }
exit:
    return mode;
}

static ipv4_mode_t ipv4_mode_from_str(const char* str) {
    ipv4_mode_t mode = IPv4_Mode_Invalid;
    when_null(str, exit);
    for(int i = 0; i < IPv4_Mode_Invalid; ++i) {
        if(0 == strcmp(str, ipv4_mode_str[i])) {
            mode = (ipv4_mode_t) i;
        }
    }
exit:
    return mode;
}