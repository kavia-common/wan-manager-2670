# TR181 compatible Wan Manager

## Installation

You can build and install the Wan Manager by running
```
make && sudo make install
```

## General requirements
* A single wan-mode interface can have two different modes for IPv4Mode and IPv6Mode
* A single IP interface can not be used by multiple wan-mode interfaces in the same wan-mode
* Only one wan-mode interface can have the "DefaultInterface" parameter set to true

## Interactions with other plugins
### IP-manager
The wan-manager makes quite a few changes to the IP-manager when switching wan-modes. In order to do these the wan-manager makes a few assumptions on the configuration of the IP-manager.

#### Requirements
* Every IP interface that is configured as a "IPv4Reference" has an IPv4Address instance that is named "primary".
  * This is the IPv4 address instance that will be configured by the wan-manager when a mode is enabled
  * This instance should not contain any manual configuration since these will be lost when switching wan-mode
* Every IP interface that is configured as a "IPv6Reference", is correctly configured with the required IPv6 addresses and IPv6 prefixes
  * The only exceptions are static IPv6 addresses, for these the wan-manager will create new instances. These instances will be named "wan-mngr", possibly suffixed with an index.

#### Changes made
All changes will be applied to the IP interface instances referenced by the IPvXReference parameters
* The LowerLayers parameter will be set, wan-manager will determine what the correct lower layer path is.
* IPv4
  * Toggle the primary IPv4 Address instance
  * Toggle IPv4 on the referenced IP interface
* IPv6
  * For static addresses new IPv6Address instances are created
  * Toggle IPv6 on the referenced IP interface
* Toggle the referenced IP interface

### DHCPv4
When switching to or from a mode that has "dhcp4" set as IPv4Mode, the wan-manager will make configuration changes to the DHCPv4 client.

#### Requirements
* A wan-mode interface with IPv4Mode set to "dhcp4" should have its DHCPv4Reference parameter filled in to point to the associated DHCPv4 client instance.

#### Changes made
* The DHCP client instance referred to by DHCPv4Reference:
  * will be enabled
  * will have its Interface parameter set

### DHCPv6
When switching to or from a mode that has "dhcp6" or "ppp6" set as IPv6Mode or "dslite" as IPv4Mode, the wan-manager will make configuration changes to the DHCPv6 client.

#### Requirements
* A wan-mode interface with IPv6Mode set to "dhcp6" should have its DHCPv6Reference parameter filled in to point to the associated DHCPv6 client instance.

#### Changes made
* The DHCP client instance referred to by DHCPv6Reference:
  * will be enabled
  * will have its Interface parameter set

### PPP
When switching to or from a mode that has "ppp4" as IPv4Mode or "ppp6" as IPv6Mode, the wan-manager will make configuration changes to the PPP plugin.

### Requirement
* A PPP interface with following path should exist "Device.PPP.Interface.1."
  * This is the interface the wan-manager will use and reconfigure

#### Changes made
* Set LowerLayers in PPP-manager
* Overrides the default PPP credentials, only if they are set in wan-manager data model
* Toggle IPCPEnable, if the IPv4Mode is ppp4
* Toggle IPCP6Enable, if the IPv6Mode is ppp6
* Toggle the PPP interface "Device.PPP.Interface.1."

### Ethernet
* No requirements

#### Changes made
A new vlan termination will be added if all the following points apply:
  * A wan-mode interface for the current wan-mode is configured to use vlans
  * No matching vlan termination is found, a match is when
    * it has the same vlanid
    * it has the same LowerLayers (lowerlayers is determined by the wan-manager it self)

## Autosensing
### Sensing parameters
The wan manager has a few parameters to configure how autosensing should behave and if it should be used or not.
These parameters are the following.
#### OperationMode
The operation mode determines if autosensing is enabled or not. There are two possible values for this parameter:
* Manual: In this mode, autosensing will be disabled. The wan mode needs to be configured through the setWANMode function or by writing to the WANMode parameter directly
* Automatic: Autosensing is enabled. The wan mode should no longer be set manually. The sensing behavior is now depending on the SensingPolicy parameter

#### SensingPolicy
This parameter only has effect when the OperationMode parameter is set to "Automatic", otherwise it is ignored.
There are two possible values for this parameter:
* AtBoot: Sensing will cycle through all eligible wan modes, it will stop as soon as an active wan mode is found or when all wan modes marked for sensing are tested once and no active mode was found. Sensing will only start during the boot sequence or when switching from Manual to Automatic mode.
* Continuous: Sensing will cycle through all eligible wan modes, it will only stop when an active wan mode is found.
  Sensing will start:
    * During boot
    * When the SensingPolicy is switched from AtBoot to Continuous
    * When OperationMode is switched from manual to Automatic
    * When a mode is enabled/disabled for sensing

#### SensingTimeout
When autosensing is sensing a mode, it will sense it for the amount of seconds set in the SensingTimeout parameter.
If the mode does not become active within this time the next mode will be selected.
Should a mode become active before the time runs out it will be kept and sensing will stop at that moment.
### When is a mode active
Currently a mode is considered active if the first interface in this mode has an IPv4 address.

### Which modes are sensed?
Only modes that are marked for sensing will be sensed.
A mode is marked for sensing if the EnableSensing parameter is set to true.

### What order will the modes sensed in?
The enabled modes will be sensed in a specific order:
1. If the current mode is enabled for sensing, it will be sensed first
2. If it is not enabled for sensing or not active (yet), the mode with the lowest index will be selected
3. If the mode is not active, the next mode will be the one with the lowest index after the current one
4. This will repeat until an active mode is found or all the modes are sensed
5. * SensingPolicy=AtBoot: Sensing will stop, the last mode that was tested will be kept
   * SensingPolicy=Continuous: The mode with the lowest index (same as in step 2) will be selected again and the process will start again

### When is the sensing stopped?
There are a few cases that will stop the sensing:
* An active mode is found
* If OperationMode is switched from Automatic to Manual
* If SensingPolicy is switched from Continuous to AtBoot
* If sensing is disabled on all wan modes
* If SensingPolicy is set to AtBoot and
  * no active mode is found after sensing all eligible modes
  * a mode is enabled/disabled for sensing
* If it fails to set the next mode, sensing will be stopped and the status for the mode will go to 'Error'

### NOTICES
* When using autosensing, changes made to the WANMode parameter directly will be ignored. This will result in the configured WANMode not matching with the mode set in the data model. If the user want to change the mode, they need to use the setWANMode function or manually switch back to manual mode.
