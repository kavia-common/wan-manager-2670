/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#include <stdlib.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "ctrl/mode_ctrl.h"
#include "component.h"
#include "wan_manager_utils.h"
#include "link/link.h"

#define ME "link-ctrl"

amxd_status_t link_enable(mode_ctrl_t mode,
                          const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    int ipmode = mode & (MASK_IPv4 | MASK_IPv6) & MASK_LINK;
    int ip_version = ((ipmode & MASK_IPv4) != 0) ? 4 : 6;
    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = ip_version == 4 ? GET_CHAR(parameters, "IPv4Reference") : GET_CHAR(parameters, "IPv6Reference");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter for interface %s is empty", intf_path);

    // Add the IPReference to the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_add_string_to_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to add IPv%dReference to '%s'", ip_version, logical_path);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}

amxd_status_t link_disable(mode_ctrl_t mode,
                           const amxc_var_t* const parameters) {
    SAH_TRACEZ_IN(ME);
    int ipmode = mode & (MASK_IPv4 | MASK_IPv6) & MASK_LINK;
    int ip_version = ((ipmode & MASK_IPv4) != 0) ? 4 : 6;

    amxd_status_t rc = amxd_status_unknown_error;
    const char* intf_path = ip_version == 4 ? GET_CHAR(parameters, "IPv4Reference") : GET_CHAR(parameters, "IPv6Reference");
    const char* name = GET_CHAR(parameters, "Name");
    char* logical_path = NULL;

    when_str_empty_trace(intf_path, exit, ERROR, "No IP interface path found");
    when_str_empty_trace(name, exit, ERROR, "Name parameter of %s is empty", intf_path);

    //Remove the IPReference from the Logical Interface
    logical_path = create_logical_path(name);
    rc = component_remove_string_from_csv(logical_path, logical_get_context(), "LowerLayers", intf_path);
    when_failed_trace(rc, exit, ERROR, "Failed to remove IPv%dReference from '%s.LowerLayers'", ip_version, logical_path);

exit:
    free(logical_path);
    SAH_TRACEZ_OUT(ME);
    return rc;
}
