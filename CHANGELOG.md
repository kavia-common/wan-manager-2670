# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


## Release v0.19.2 - 2023-05-11(09:20:17 +0000)

### Other

- [Coverage] Remove SAHTRACE defines in order to increase branching coverage

## Release v0.19.1 - 2023-04-27(14:16:28 +0000)

### Fixes

- Fix component not starting on openwrt22

## Release v0.19.0 - 2023-04-27(09:51:14 +0000)

### New

- [wan-manager] PPPv6 modes should send a DHCPv6 IA_PD solicit

## Release v0.18.3 - 2023-04-17(08:05:44 +0000)

### Fixes

- Issu: HOP-3320 [odl]Remove deprecated odl keywords

## Release v0.18.2 - 2023-03-26(07:56:09 +0000)

### Fixes

- Should not toggle wanmode at boot

## Release v0.18.1 - 2023-03-15(11:07:35 +0000)

### Fixes

- lan parent prefixes are not updated when switching wan-modes

## Release v0.18.0 - 2023-03-13(15:54:59 +0000)

### Fixes

- [DHCPv4] Split client and server plugin

## Release v0.17.1 - 2023-03-09(12:01:04 +0000)

### Other

- [Config] enable configurable coredump generation

## Release v0.17.0 - 2023-03-08(09:50:55 +0000)

### New

- Implement DSLite + PPP6 WANMode

## Release v0.16.2 - 2023-03-07(13:19:40 +0000)

### Fixes

- [wan-manager] DSLite static default route is incorrectly modified when switching WANModes

### Other

- Add missing runtime dependency on rpcd

## Release v0.16.1 - 2023-02-24(08:52:56 +0000)

### Other

- Documentation generation fails

## Release v0.16.0 - 2023-02-18(07:25:54 +0000)

### New

- Implement a replacement for the LastWANMode from autosensing

## Release v0.15.0 - 2023-02-14(13:19:49 +0000)

### New

- implement the getWANMode function

## Release v0.14.3 - 2023-02-14(12:17:00 +0000)

### Fixes

- [tr181-ppp] Sometimes there is no default route for ppp6

## Release v0.14.2 - 2023-02-10(10:48:23 +0000)

### Fixes

- Improve stability

## Release v0.14.1 - 2023-02-02(14:55:24 +0000)

### Fixes

- Adapt the Wan-Manager to use DNS management functions of the TR181-DNS plugin

## Release v0.14.0 - 2023-02-02(08:59:20 +0000)

### New

- Enable PCP when WANMode == DSLite

## Release v0.13.1 - 2023-01-27(09:50:37 +0000)

### Other

- Configure static IPv4 and IPv6 addresses trough the wan-manager

## Release v0.13.0 - 2023-01-26(12:44:30 +0000)

### New

- [ppp][ipv6] It must be possible to support pppv6 (ip6cp) with the ppp plugin

## Release v0.12.0 - 2023-01-23(08:37:00 +0000)

### New

- [ipv6][dslite][wanmanager] Add support for dslite in the WANManager

## Release v0.11.0 - 2023-01-21(08:27:09 +0000)

### New

- Prepare debian package + changelog

## Release v0.10.0 - 2023-01-16(13:56:28 +0000)

### New

- Set/clear Logical.Interface.X in WANManager

## Release v0.9.0 - 2023-01-13(12:03:36 +0000)

### New

- Implement autosensing directly in the DM/plugin

## Release v0.8.0 - 2023-01-12(10:31:42 +0000)

### New

- Add the possibility to configure the DNS (Mode + Servers)

## Release v0.7.4 - 2023-01-10(11:07:45 +0000)

### Fixes

- [Wan Manager] PPP credentials not used

## Release v0.7.3 - 2023-01-09(11:54:14 +0000)

### Fixes

- RoutingManager does not get its interface reference updated when switching modes in wan-manager

## Release v0.7.2 - 2023-01-09(09:22:44 +0000)

### Fixes

- avoidable copies of strings, htables and lists

## Release v0.7.1 - 2022-12-19(16:15:22 +0000)

### Fixes

- LLA address does not come back to IP.Interface.2. when switching back from ppp-mode to wan-mode

## Release v0.7.0 - 2022-12-15(08:22:06 +0000)

### New

- [WANMode] Add default WANModes (Ethernet_DHCP and Ethernet_PPP)

## Release v0.6.0 - 2022-12-09(14:14:57 +0000)

### New

- Create support for mixed IPModes

## Release v0.5.1 - 2022-12-09(09:31:59 +0000)

### Fixes

- [Config] coredump generation should be configurable

## Release v0.5.0 - 2022-11-17(09:11:46 +0000)

### New

- [WAN-Manager] Not possible to use Ethernet_PPP with vlan

## Release v0.4.0 - 2022-09-15(08:11:49 +0000)

### New

- Add a configuration for the Ethernet_PPP WANmode

## Release v0.3.4 - 2022-07-15(12:09:10 +0000)

### Fixes

- No dhcp v4 address on LAN

## Release v0.3.3 - 2022-06-30(08:21:31 +0000)

### Fixes

- Startup after getting lowerlayer name

## Release v0.3.2 - 2022-06-20(12:43:48 +0000)

### Fixes

- [WANManager] Update lower layer when creating new wanmodes

## Release v0.3.1 - 2022-06-09(08:11:35 +0000)

### Fixes

- [WANManager] vlan not disabled when switching from vlan to untagged

## Release v0.3.0 - 2022-06-03(09:48:26 +0000)

### New

- [WANManager] Add IPv6Mode = dhcp6 functionality

## Release v0.2.10 - 2022-05-30(12:15:20 +0000)

### Other

- [WANManager] make odl persistent

## Release v0.2.9 - 2022-05-24(14:49:03 +0000)

### Fixes

- [WANManager] use interface path as reference to find dhcpv4/6 client instead of alias

## Release v0.2.8 - 2022-05-23(11:54:24 +0000)

### Fixes

- getCurrentWANModeStatus function always returns false

## Release v0.2.7 - 2022-05-23(07:42:51 +0000)

### Fixes

- [Gitlab CI][Unit tests][valgrind] Pipeline doesn't stop when memory leaks are detected

## Release v0.2.6 - 2022-05-04(15:07:04 +0000)

### Fixes

- Change default Alias of WAN.demo_vlanmode.Intf to wan

## Release v0.2.5 - 2022-04-05(13:11:07 +0000)

### Fixes

- Switch between modes failing

## Release v0.2.4 - 2022-03-24(10:03:07 +0000)

### Changes

- [GetDebugInformation] Add data model debuginfo in component services

## Release v0.2.3 - 2022-03-22(16:42:49 +0000)

### Changes

- Issue:  HOP-1208  [WAN-Manager] WAN Manager cleanup

## Release v0.2.2 - 2022-03-22(16:18:42 +0000)

### Changes

- [amx][WANAutosensing] [WANManager] Use proper vendor extension prefix

## Release v0.2.1 - 2022-02-25(11:11:55 +0000)

### Other

- Enable core dumps by default

## Release v0.2.0 - 2021-12-21(08:35:15 +0000)

### New

- Integrate with NetDev
- Integrate with tr181-dhcpv4client and tr181-ethernet-manager

## Release v0.1.1 - 2021-11-15(09:03:06 +0000)

### Fixes

- Missing dependency to libsahtrace
- Missing mod-sahtrace dependecy

### Other

- Extend plugin description

## Release v0.1.0 - 2021-08-24(13:56:55 +0000)

### New

- PCF-191: Initial DM. Backend logic. Component ctrl intf
