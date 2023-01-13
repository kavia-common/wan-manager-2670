# TR181 compatible Wan Manager

## Installation

You can build and install the Wan Manager by running
```
make && sudo make install
```

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
* When using autosensing, changes made to the WANMode parameter directly will be ignored. This will result in the configured WANMode not matching with the mode set in the datamodel. If the user want to change the mode, they need to use the setWANMode function or manually switch back to manual mode.
