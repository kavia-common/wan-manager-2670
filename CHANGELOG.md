# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


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
