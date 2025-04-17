/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#if !defined(__DM_WAN_MANAGER_H__)
#define __DM_WAN_MANAGER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>
#include <amxb/amxb.h>
#include <amxo/amxo.h>

#define MOD_DM_MNGR   "dm-mngr"

typedef struct  {
    amxd_dm_t* dm;
    amxo_parser_t* parser;
    amxb_bus_ctx_t* context;
} wan_manager_app_t;

int _wan_manager_main(int reason,
                      amxd_dm_t* dm,
                      amxo_parser_t* parser);

amxd_dm_t* PRIVATE wan_get_dm(void);

amxo_parser_t* PRIVATE wan_get_parser(void);

amxd_status_t _WANModeEnable(amxd_object_t* object,
                             amxd_function_t* func,
                             amxc_var_t* args,
                             amxc_var_t* ret);

amxd_status_t _WANModeDisable(amxd_object_t* object,
                              amxd_function_t* func,
                              amxc_var_t* args,
                              amxc_var_t* ret);

amxd_status_t _setWANMode(amxd_object_t* object,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _setIPv4Mode(amxd_object_t* object,
                           amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret);

amxd_status_t _setIPv6Mode(amxd_object_t* object,
                           amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret);

amxd_status_t _Reset(amxd_object_t* object,
                     amxd_function_t* func,
                     amxc_var_t* args,
                     amxc_var_t* ret);

amxd_status_t _getWANMode(amxd_object_t* object,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);


void _set_wan_mode(const char* const event_name,
                   const amxc_var_t* const event_data,
                   void* const priv);

amxd_status_t _check_wan_mode(amxd_object_t* object,
                              amxd_param_t* param,
                              amxd_action_t reason,
                              const amxc_var_t* const args,
                              amxc_var_t* const retval,
                              void* priv);

amxd_status_t _interface_already_configured(amxd_object_t* object,
                                            amxd_param_t* param,
                                            amxd_action_t reason,
                                            const amxc_var_t* const args,
                                            amxc_var_t* const retval,
                                            void* priv);

amxd_status_t is_valid_mode_list(const char* new_wan_mode);

void _dm_wan_manager_physical_type_changed(const char* const event_name,
                                           const amxc_var_t* const event_data,
                                           void* const priv);

void _wan_sensing_priority_changed(const char* const event_name,
                                   const amxc_var_t* const event_data,
                                   void* const priv);

void _dm_wan_manager_wan_added(const char* const event_name,
                               const amxc_var_t* const event_data,
                               void* const priv);

amxd_status_t _wan_destroy(amxd_object_t* intf,
                           amxd_param_t* param,
                           amxd_action_t reason,
                           const amxc_var_t* const args,
                           amxc_var_t* const retval,
                           void* priv);

amxd_status_t _interface_destroy(amxd_object_t* intf,
                                 amxd_param_t* param,
                                 amxd_action_t reason,
                                 const amxc_var_t* const args,
                                 amxc_var_t* const retval,
                                 void* priv);

void _app_start(const char* const event_name,
                const amxc_var_t* const event_data,
                void* const priv);

#ifdef __cplusplus
}
#endif

#endif // __DM_WAN_MANAGER_H__
