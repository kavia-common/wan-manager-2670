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
#if !defined(__UTILS_H__)
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <amxb/amxb.h>
#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>

#define ADDRESSING_TYPE_DHCP "DHCP"
#define ADDRESSING_TYPE_PPP "IPCP"
#define ADDRESSING_TYPE_STATIC "Static"
#define ADDRESSING_TYPE_3GPP_NAS "3GPP-NAS"
#define ADDRESSING_TYPE_AUTO "AutoConfigured"

#define CHILD_PREFIX_TYPE "Child"
#define STATIC_PREFIX_TYPE "Static"

#define ROUTING_ORIGIN_DHCPV4 "DHCPv4"
#define ROUTING_ORIGIN_IPCP "IPCP"
#define ROUTING_ORIGIN_RIP "RIP"
#define ROUTING_ORIGIN_OSPF "OSPF"
#define ROUTING_ORIGIN_STATIC "Static"
#define ROUTING_ORIGIN_3GPP_NAS "3GPP-NAS"
#define ROUTING_ORIGIN_AUTOMATIC "Automatic"

#define DEVICE_PATH "Device."
#define DHCPV4_PATH "DHCPv4Client."
#define DHCPV6_PATH "DHCPv6Client."
#define DNS_PATH "DNS."
#define PPP_PATH "PPP."
#define DSLITE_PATH "DSLite."
#define PCP_PATH "PCP."

#define IPV4_REFERENCE_PATH "IPv4Reference"
#define IPV6_REFERENCE_PATH "IPv6Reference"
#define DHCPV4_REFERENCE_PATH "DHCPv4Reference"
#define DHCPV6_REFERENCE_PATH "DHCPv6Reference"
#define PPPV4_REFERENCE_PATH "PPPv4Reference"
#define PPPV6_REFERENCE_PATH "PPPv6Reference"
#define DSLITE_REFERENCE_PATH "DSLiteReference"
#define PCP_REFERENCE_PATH "PCPReference"
#define NEIGH_REFERENCE_PATH "NeighborDiscoveryReference"

#define STATIC_CONF_INTF_ALIAS "lan"
#define STATIC_CONF_PREFIX_ALIAS "GUA_STATIC"

#define str_empty(txt) ((txt == NULL) || (*txt == 0))

typedef enum  {
    IPv4 = 4,
    IPv6 = 6,
    IPvInvalid = 100,
} ipversion_t;

bool ipversion_valid(const ipversion_t ip_version);

amxb_bus_ctx_t* ip_get_context(void);
amxb_bus_ctx_t* dhcpv4_get_context(void);
amxb_bus_ctx_t* dhcpv6_get_context(void);
amxb_bus_ctx_t* ppp_get_context(void);
amxb_bus_ctx_t* routing_get_context(void);
amxb_bus_ctx_t* dns_get_context(void);
amxb_bus_ctx_t* ethernet_get_context(void);
amxb_bus_ctx_t* dslite_get_context(void);
amxb_bus_ctx_t* logical_get_context(void);
amxb_bus_ctx_t* neighbor_discovery_get_context(void);
amxb_bus_ctx_t* pcp_get_context(void);
amxb_bus_ctx_t* xpon_get_context(void);
amxb_bus_ctx_t* cellular_get_context(void);

const char* get_ip_path(const amxc_var_t* const parameters, const ipversion_t ip_version, bool prefixed);
const char* get_ppp_path(const amxc_var_t* const parameters, const ipversion_t ip_version, bool prefixed);
char* get_dhcp_path(const amxc_var_t* const parameters, const ipversion_t ip_version);
const char* get_nd_path(const amxc_var_t* const parameters);
const char* get_dslite_path(const amxc_var_t* const parameters);
const char* get_pcp_path(const amxc_var_t* const parameters);
const char* get_bridge_path(const amxc_var_t* const parameters);
const char* get_default_router_path(const amxc_var_t* const parameters);

amxd_status_t ipv6_addr_toggle(const char* intf_path, amxc_var_t* ip_addr, bool enable);
amxd_status_t ipv4_addr_toggle(const char* intf_path, amxc_var_t* ip_addr, const char* addr_type, bool enable);
amxd_status_t ipv6_prefix_toggle(const char* intf_path, amxc_var_t* ip_addr, const char* addr_type, bool enable);
amxd_status_t ipv6_prefix_lan_toggle(const char* intf_alias, const char* prefix_alias, bool enable);
int ip_parent_prefix_toggle(const char* interfaces, const char* old_reference_path, const char* new_reference_path);
amxd_status_t routing_default_route_set_origin(const char* route_path, const char* ip_path, const char* routing_origin, const char* ip_addr);
amxd_status_t routing_default_ipv6_route_mod_inst(const char* routing_origin, const char* next_hop, const char* ip_intf, bool enable);
char* routing_get_interfacesetting(const char* intf_path);
void add_str_to_list(amxc_var_t* list, const char* str);
void remove_str_from_list(amxc_var_t* list, const char* str);
char* create_logical_path(const char* intf_name, bool prefixed);

const char* object_const_string(amxd_object_t* object, const char* name);
char* trim_final_dot(const char* path);

char* get_prefixed_parameter_name(const char* parameter_name);

#ifdef __cplusplus
}
#endif

#endif // __UTILS_H__
