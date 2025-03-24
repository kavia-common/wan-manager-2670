# **WAN Manager Demos**
- [Demo scenarios](#wanmanagerdemos-demoscenarios) 
  - [Preparations](#wanmanagerdemos-preparations) 
    - [Required hardware](#wanmanagerdemos-requiredhardware)
    - [Required config changes](#wanmanagerdemos-requiredconfigchanges)
    - [Known limitations](#wanmanagerdemos-knownlimitations)
  - [Set Cellular WANMode](#wanmanagerdemos-setcellularwanmode) 
    - [Prerequisites](#wanmanagerdemos-prerequisites)
    - [Reproduction steps](#wanmanagerdemos-reproductionsteps)
    - [Results](#wanmanagerdemos-results)
  - [Set both Ethernet and Cellular WANMode](#wanmanagerdemos-setbothethernetandcellularwanmode) 
    - [Prerequisites](#wanmanagerdemos-prerequisites.1)
    - [Reproduction Steps](#wanmanagerdemos-reproductionsteps)
    - [Results](#wanmanagerdemos-results.1)
  - [Fallback to Cellular when Ethernet fails on Continuous sensing policy](#wanmanagerdemos-fallbacktocellularwhenethernetfailsoncontinuoussensingpolicy) 
    - [Prerequisites](#wanmanagerdemos-prerequisites.2)
    - [Reproduction Steps](#wanmanagerdemos-reproductionsteps.1)
  - [Autosensing + Cellular: Reboot HGW, unplug ethernet with demo_wanmode active, automatic switch to Cellular after reboot](#wanmanagerdemos-autosensing+cellular:reboothgw,unplugethernetwithdemo_wanmodeactive,automaticswitchtocellularafterreboot) 
    - [Prerequisites](#wanmanagerdemos-prerequisites.3)
    - [Reproduction Steps](#wanmanagerdemos-reproductionsteps.2)
  - [No changes when setting Policy to AtBoot](#wanmanagerdemos-nochangeswhensettingpolicytoatboot) 
    - [Prerequisites](#wanmanagerdemos-prerequisites.4)
    - [Reproduction Steps](#wanmanagerdemos-reproductionsteps.3)
  - [AutoSensing not started when changing WANMode manually + Sticky Policy + Changing Priority](#wanmanagerdemos-autosensingnotstartedwhenchangingwanmodemanually+stickypolicy+changingpriority) 
    - [Prerequisites](#wanmanagerdemos-prerequisites.5)
    - [Reproduction Steps](#wanmanagerdemos-reproductionsteps.4)

This page intends to track the recent and future WAN Manager changes accompanied with scenarios to demo these changes.
# <a name="wanmanagerdemos-demoscenarios"></a>**Demo scenarios**
## <a name="wanmanagerdemos-preparations"></a>**Preparations**
### <a name="wanmanagerdemos-requiredhardware"></a>**Required hardware**
- For Cellular WANModes: Cellular dongle + SIM card
### <a name="wanmanagerdemos-requiredconfigchanges"></a>**Required config changes**
For Cellular WANModes:

- The "/etc/networklayout.json" file should have a wwan interface among the list of Interfaces:

{"Alias": "WWAN0","Name": "wwan0","Type": "cellular","Upstream": "true", "Failover": "true"},


If you don't want to demo the cellular changes, you only need a recent version of wan-manager, at least version master\_v1.8.0 (introduction of PhysicalReference parameter).
### <a name="wanmanagerdemos-knownlimitations"></a>**Known limitations**
|**Issue**|**Workaround**|
| :-: | :-: |
|After flashing image the modem manager might not find the dongle|Reboot the board|
|If cellular connection is idle for a long time, the link can get disabled by the cell tower|Delete bearer from modem manager and let cellular manager create a new one:<br>mmcli -m X --delete-bearer=Y (where X is modem ID and Y is bearer ID)|
## <a name="wanmanagerdemos-setcellularwanmode"></a>**Set Cellular WANMode**
### <a name="wanmanagerdemos-prerequisites"></a>**Prerequisites**
- APN needs to be set correctly: 
```
   - ubus: - [ubus-cli] (2)

   > Cellular.AccessPoint.1.APN?

  Cellular.AccessPoint.1.APN="apn"
```
- Make sure the modem manager finds the usb dongle. If not found, see Known limitations above:
```
  root@prplOS:/# mmcli -L 0

      /org/freedesktop/ModemManager1/Modem/0
```
### <a name="wanmanagerdemos-reproductionsteps"></a>**Reproduction steps**
```
WANManager.setWANMode(WANMode = Cellular\_IPv4)

WANManager.setWANMode() returned

[

    {

        status = 1

    }

]
```
### <a name="wanmanagerdemos-results"></a>**Results**
|**Datamodel Path (where IP.Interface.12 is the failover Interface)**|**Expected Value**|
| :-: | :-: |
|IP.Interface.12.IPv4Address.1.AddressingType|3GPP-NAS|
|Routing.Router.1.IPv4Forwarding.1.Origin|3GPP-NAS|
|Routing.Router.1.IPv4Forwarding.1.Interface|Device.IP.Interface.12.|
|Logical.Interface.1.LowerLayers |Device.IP.Interface.12.|
```
root@prplOS:/# mmcli -b 0

  ------------------------------------

  General            |           path: /org/freedesktop/ModemManager1/Bearer/0

                     |           type: default

  ------------------------------------

  Status             |      connected: yes

                     |      suspended: no

                     |    multiplexed: no

                     |      interface: wwan0

                     |     ip timeout: 20

  ------------------------------------

  Properties         |            apn: apn

                     |        roaming: forbidden

  ------------------------------------

  IPv4 configuration |         method: static

                     |        address: 10.185.68.101

                     |         prefix: 30

                     |        gateway: 10.185.68.102

                     |            dns: 212.224.129.94, 212.224.129.90

  ------------------------------------

  Statistics         |     start date: 2025-02-13T15:36:41Z

                     |       duration: 599

                     |       attempts: 1

                     | total-duration: 599

root@prplOS:/# ip a s wwan0

13: wwan0: <BROADCAST,MULTICAST,UP,LOWER\_UP> mtu 1500 qdisc fq\_codel state UNKNOWN group default qlen 1000

`    `link/ether ac:91:9b:3a:5d:0c brd ff:ff:ff:ff:ff:ff

`    `inet 10.185.68.101/30 scope global wwan0

`       `valid\_lft forever preferred\_lft forever

`    `inet6 fe80::ae91:9bff:fe3a:5d0c/64 scope link 

`       `valid\_lft forever preferred\_lft forever

root@prplOS:/# ip r

default via 10.185.68.102 dev wwan0 

10\.185.68.100/30 dev wwan0 proto kernel scope link src 10.185.68.101 

192\.168.1.0/24 dev br-lan proto kernel scope link src 192.168.1.1 

192\.168.2.0/24 dev br-guest proto kernel scope link src 192.168.2.1 

192\.168.5.0/24 dev br-lcm proto kernel scope link src 192.168.5.1

root@prplOS:/# ping google.com

PING google.com (74.125.71.102) 56(84) bytes of data.

64 bytes from wn-in-f102.1e100.net (74.125.71.102): icmp\_seq=1 ttl=56 time=28.6 ms

64 bytes from wn-in-f102.1e100.net (74.125.71.102): icmp\_seq=2 ttl=56 time=37.5 ms

64 bytes from wn-in-f102.1e100.net (74.125.71.102): icmp\_seq=3 ttl=56 time=34.0 ms

^C

--- google.com ping statistics ---

3 packets transmitted, 3 received, 0% packet loss, time 2003ms

rtt min/avg/max/mdev = 28.634/33.377/37.475/3.638 ms

```
## <a name="wanmanagerdemos-setbothethernetandcellularwanmode"></a>**Set both Ethernet and Cellular WANMode**
### <a name="wanmanagerdemos-prerequisites.1"></a>**Prerequisites**
- Cellular\_IPv4 WANMode set
- NOTE: it's also possible to start from demo\_wanmode and add the Cellular\_IPv4 WANMode but this would mostly be the same steps as the previous demo.
### **Reproduction Steps**
- First we need to add a new Routing.Router.IPv4Forwarding object since the default one is already used by the cellular WANMode:
```
 - ubus: - [ubus-cli] (0)

 > Device.Routing.Router.1.IPv4Forwarding.+{Interface="Device.IP.Interface.2.",Enable=1, ForwardingMetric=10}

Device.Routing.Router.1.IPv4Forwarding.16.

Device.Routing.Router.1.IPv4Forwarding.16.Alias="cpe-IPv4Forwarding-16"

 - ubus: - [ubus-cli] (0)

 > WANManager.WAN.demo\_wanmode.Intf.1.DefaultRouteReference="Device.Routing.Router.1.IPv4Forwarding.16."

WANManager.WAN.1.Intf.1.

WANManager.WAN.1.Intf.1.DefaultRouteReference="Device.Routing.Router.1.IPv4Forwarding.16."

- Also the default route of the wwan0 interface should have a Metric set: 

   - ubus: - [ubus-cli] (0)

   > Routing.Router.1.IPv4Forwarding.1.ForwardingMetric=20

  Routing.Router.1.IPv4Forwarding.1.

  Routing.Router.1.IPv4Forwarding.1.ForwardingMetric=20

- Next enable demo\_wanmode: 

   - ubus: - [ubus-cli] (0)

   > WANManager.WANModeEnable(WANMode = demo\_wanmode)

  WANManager.WANModeEnable() returned

  [

     {

          status = 1

      }

  ]
```
### <a name="wanmanagerdemos-results.1"></a>**Results**
```
root@prplOS:/# ip a s wwan0

13: wwan0: <BROADCAST,MULTICAST,UP,LOWER\_UP> mtu 1500 qdisc fq\_codel state UNKNOWN group default qlen 1000

    link/ether ac:91:9b:3a:5d:0c brd ff:ff:ff:ff:ff:ff

    inet 10.185.68.101/30 scope global wwan0

       valid\_lft forever preferred\_lft forever

    inet6 fe80::ae91:9bff:fe3a:5d0c/64 scope link 

       valid\_lft forever preferred\_lft forever

4: eth1: <BROADCAST,MULTICAST,UP,LOWER\_UP> mtu 1500 qdisc nsshtb state UP group default qlen 1000

    link/ether ac:91:9b:3a:5d:0c brd ff:ff:ff:ff:ff:ff

    inet 172.16.120.199/24 scope global eth1

       valid\_lft forever preferred\_lft forever

    inet6 2a02:1802:94:4200:ae91:9bff:fe3a:5d0c/64 scope global 

       valid\_lft forever preferred\_lft forever

    inet6 fe80::ae91:9bff:fe3a:5d0c/64 scope link 

       valid\_lft forever preferred\_lft forever


root@prplOS:/# ip r

default via 172.16.120.10 dev eth1 metric 10

default via 10.185.68.102 dev wwan0 metric 20

10\.185.68.100/30 dev wwan0 proto kernel scope link src 10.185.68.101 

68\.68.68.0/24 via 172.16.120.10 dev eth1 

69\.69.69.0/24 via 172.16.120.10 dev eth1 

172\.16.120.0/24 dev eth1 proto kernel scope link src 172.16.120.199 

192\.168.1.0/24 dev br-lan proto kernel scope link src 192.168.1.1 

192\.168.2.0/24 dev br-guest proto kernel scope link src 192.168.2.1 

192\.168.5.0/24 dev br-lcm proto kernel scope link src 192.168.5.1 

root@prplOS:/# ping4 google.com

PING google.com (64.233.167.138) from 10.211.52.78 wwan0: 56(84) bytes of data.

64 bytes from wl-in-f138.1e100.net (64.233.167.138): icmp\_seq=1 ttl=56 time=140 ms

64 bytes from wl-in-f138.1e100.net (64.233.167.138): icmp\_seq=2 ttl=56 time=57.9 ms

64 bytes from wl-in-f138.1e100.net (64.233.167.138): icmp\_seq=3 ttl=56 time=36.8 ms

64 bytes from wl-in-f138.1e100.net (64.233.167.138): icmp\_seq=4 ttl=56 time=35.0 ms

64 bytes from wl-in-f138.1e100.net (64.233.167.138): icmp\_seq=5 ttl=56 time=35.0 ms

^C

--- google.com ping statistics ---

5 packets transmitted, 5 received, 0% packet loss, time 4005ms

rtt min/avg/max/mdev = 34.974/60.992/140.266/40.568 ms

root@prplOS:/# ping6 google.com

PING google.com(wa-in-x66.1e100.net (2a00:1450:400c:c0b::66)) 56 data bytes

64 bytes from wa-in-x66.1e100.net (2a00:1450:400c:c0b::66): icmp\_seq=1 ttl=57 time=18.9 ms

64 bytes from wa-in-x66.1e100.net (2a00:1450:400c:c0b::66): icmp\_seq=2 ttl=57 time=17.6 ms

64 bytes from wa-in-f102.1e100.net (2a00:1450:400c:c0b::66): icmp\_seq=3 ttl=57 time=19.8 ms

64 bytes from wa-in-x66.1e100.net (2a00:1450:400c:c0b::66): icmp\_seq=4 ttl=57 time=14.3 ms

^C

--- google.com ping statistics ---

4 packets transmitted, 4 received, 0% packet loss, time 3004ms
```
## <a name="wanmanagerdemos-fallbacktocellularwhenethernetfailsoncontinuoussensingpolicy"></a>**Fallback to Cellular when Ethernet fails on Continuous sensing policy**
### <a name="wanmanagerdemos-prerequisites.2"></a>**Prerequisites**
- Ethernet Cable plugged in in WAN port
- Cellular dongle plugged in
- Unique Routing.Router.IPv4Forwarding instances for Cellular and Ethernet are set for demo\_wanmode and Cellular\_IPv4
- WANManager.OperationMode set to "Automatic"
- WANManager.SensingPolicy set to "Continuous"
- WANManager.WANMode set to "demo\_wanmode"
- WANManager.WAN.demo\_wanmode.SensingPriority set to 50 and WANManager.WAN.demo\_wanmode.EnableSensing=1
- WANManager.WAN.Cellular\_IPv4.SensingPriority set to 100 and WANManager.WAN.Cellular\_IPv4.EnableSensing=1
- Other WANmodes can have EnableSensing set to 0
### <a name="wanmanagerdemos-reproductionsteps.1"></a>**Reproduction Steps**
- Remove Ethernet cable from WAN Port 

  [2025-02-28T13:39:21Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.Status = Enabled -> Disabled

  [2025-02-28T13:39:21Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.WANMode = demo\_wanmode -> Cellular\_IPv4

  [2025-02-28T13:39:21Z] Event dm:object-changed received from WANManager.WAN.9.

  <  dm:object-changed> WANManager.WAN.9.Status = Disabled -> Enabled

- Plugin Ethernet cable again 

  [2025-02-28T13:40:30Z] Event dm:object-changed received from WANManager.WAN.9.

  <  dm:object-changed> WANManager.WAN.9.Status = Enabled -> Disabled

  [2025-02-28T13:40:30Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.WANMode = Cellular\_IPv4 -> demo\_wanmode

  [2025-02-28T13:40:30Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.Status = Disabled -> Enabled



## <a name="wanmanagerdemos-autosensing+cellular:reboothgw,unplugethernetwithdemo_wanmodeactive,automaticswitchtocellularafterreboot"></a>**Autosensing + Cellular: Reboot HGW, unplug ethernet with demo\_wanmode active, automatic switch to Cellular after reboot**
### <a name="wanmanagerdemos-prerequisites.3"></a>**Prerequisites**
Same as "Fallback to cellular"
### <a name="wanmanagerdemos-reproductionsteps.2"></a>**Reproduction Steps**
- Reboot
- When board is booting up again unplug the Ethernet cable.
  **Make sure to wait until the shutdown is completed, to avoid switching to Cellular before the reboot happen.**
- After reboot Autosensing should have set Cellular\_IPv4 WANMode. 
```
   > WANManager.?1

  WANManager.

  WANManager.OperationMode="Automatic"

  WANManager.SensingPolicy="Continuous"

  WANManager.SensingTimeout=8

  WANManager.WANMode="Cellular\_IPv4"

- Plugin Ethernet cable again 

  [2025-02-28T13:46:34Z] Event dm:object-changed received from WANManager.WAN.9.

  <  dm:object-changed> WANManager.WAN.9.Status = Enabled -> Disabled

  [2025-02-28T13:46:34Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.WANMode = Cellular\_IPv4 -> demo\_wanmode

  [2025-02-28T13:46:34Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.Status = Disabled -> Enabled

- fds
```
## <a name="wanmanagerdemos-nochangeswhensettingpolicytoatboot"></a>**No changes when setting Policy to AtBoot**
### <a name="wanmanagerdemos-prerequisites.4"></a>**Prerequisites**
Same as "Fallback to cellular"
### <a name="wanmanagerdemos-reproductionsteps.3"></a>**Reproduction Steps**
- Change SensingPolicy to "AtBoot"
  → Active WANMode should not change!
- Unplug Ethernet cable
  → Active WANMode should not change!
- Wait a few seconds before plugging Ethernet cable back in
  → Active WANMode should not change!
## <a name="wanmanagerdemos-autosensingnotstartedwhenchangingwanmodemanually+stickypolicy+changingpriority"></a>**AutoSensing not started when changing WANMode manually + Sticky Policy + Changing Priority**
### <a name="wanmanagerdemos-prerequisites.5"></a>**Prerequisites**
Same as "Fallback to cellular"
### <a name="wanmanagerdemos-reproductionsteps.4"></a>**Reproduction Steps**
```
- Set WANMode to Cellular\_IPv4 

  > WANManager.setWANMode(WANMode = Cellular\_IPv4, Autosensing = false)

  WANManager.setWANMode() returned

  [

  	{

  		status = 1

  	}

  ]

  [2025-02-28T13:48:47Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.OperationMode = Automatic -> Manual

  <                   > WANManager.WANMode = demo\_wanmode -> Cellular\_IPv4

  [2025-02-28T13:48:47Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.Status = Enabled -> Disabled

  [2025-02-28T13:48:47Z] Event dm:object-changed received from WANManager.WAN.9.

  <  dm:object-changed> WANManager.WAN.9.Status = Disabled -> Enabled

- Remove Ethernet Cable
  → Active WANMode should not change!
- Set SensingPolicy to "Sticky" and "OperationMode to "Automatic" 

  > WANManager.SensingPolicy="Sticky"

  WANManager.

  WANManager.SensingPolicy="Sticky"

  [2025-02-28T13:50:05Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.SensingPolicy = AtBoot -> Sticky

   - ubus: - [ubus-cli] (0)

   > WANManager.OperationMode="Automatic"

  WANManager.

  WANManager.OperationMode="Automatic"

  [2025-02-28T13:50:15Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.OperationMode = Manual -> Automatic

- Plug in Ethernet cable again
  → Active WANMode should not change!
- Set SensingPolicy back to "Continuous" 

  [2025-02-28T13:53:23Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.SensingPolicy = Sticky -> Continuous

  [2025-02-28T13:53:23Z] Event dm:object-changed received from WANManager.WAN.9.

  <  dm:object-changed> WANManager.WAN.9.Status = Enabled -> Disabled

  [2025-02-28T13:53:23Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.WANMode = Cellular\_IPv4 -> demo\_wanmode

  [2025-02-28T13:53:23Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.Status = Disabled -> Enabled

- Set SensingPriority of demo\_wanmode to 150 (higher than Cellular\_IPv4) 

  [2025-02-28T13:54:43Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.SensingPriority = 50 -> 150

  [2025-02-28T13:54:43Z] Event dm:object-changed received from WANManager.WAN.1.

  <  dm:object-changed> WANManager.WAN.1.Status = Enabled -> Disabled

  [2025-02-28T13:54:43Z] Event dm:object-changed received from WANManager.

  <  dm:object-changed> WANManager.WANMode = demo\_wanmode -> Cellular\_IPv4

  [2025-02-28T13:54:43Z] Event dm:object-changed received from WANManager.WAN.9.

  <  dm:object-changed> WANManager.WAN.9.Status = Disabled -> Enabled
```