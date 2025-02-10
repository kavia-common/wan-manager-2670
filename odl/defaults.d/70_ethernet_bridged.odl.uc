%populate {
    object 'WANManager.WAN' {
        instance add ("Ethernet_bridged"){
            parameter Alias = "Ethernet_bridged";
            parameter PhysicalType = "Ethernet";
            parameter PhysicalReference = "Device.Ethernet.Interface.{{ BDfn.getUpstreamInterfaceIndex() + 1 }}";
            parameter EnableSensing = false;
        }
    }

    object 'WANManager.WAN.Ethernet_bridged.Intf'{
        instance add(0,"wan"){
            parameter Type = "untagged";
            parameter IPv4Mode = "none";
            parameter IPv6Mode = "none";
            parameter Name = "wan";
            parameter IPv4Reference = "${ip_intf_wan}";
            parameter IPv6Reference = "${ip_intf_wan}";
            parameter BridgeReference = "Device.Bridging.Bridge.1.";
        }
    }
}
