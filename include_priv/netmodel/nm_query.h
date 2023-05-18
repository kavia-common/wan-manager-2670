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
#if !defined(__NM_QUERY_H__)
#define __NM_QUERY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_types.h>

#include <netmodel/client.h>

typedef enum _physical_type {
    physical_type_ethernet,
    physical_type_bridge,
    physical_type_adsl,
    physical_type_vdsl,
    physical_type_sfp,
    physical_type_gpon,
    physical_type_gfast,
    physical_type_wwan,
    physical_type_last
} physical_type_t;

typedef struct _nm_query_ll_info {
    netmodel_query_t* q_name;
    netmodel_query_t* q_intf_path;
    char* intf_name;
    char* lower_layer;
    char* upstream_intf_path;
    int index;
    bool used;
} nm_query_ll_info_t;

typedef struct _intf_isup_queries {
    netmodel_query_t* nm_ipv4_up_query;                 // pointer to a netmodel ipv4-up query
    netmodel_query_t* nm_ipv6_up_query;                 // pointer to a netmodel ipv6-up query
} intf_isup_queries_t;

void nm_query_ll_init(void);
void nm_query_ll_cleanup(void);
int nm_query_ll_add(const char* name);
const char* nm_query_get_lower_layer(const char* name);
int nm_query_mode_active(void);
void nm_close_sensing_queries(void);
nm_query_ll_info_t* get_nm_query_info(int index);

#ifdef __cplusplus
}
#endif

#endif // __NM_QUERY_H__
