all:
	m4 -D START_ORDER=$(CONFIG_SAH_AMX_WAN_MANAGER_ORDER) -D STOP_ORDER=$(CONFIG_SAH_AMX_WAN_MANAGER_ORDER) wan-manager.sh.m4 > wan-manager.sh

clean:
	rm -rf wan-manager.sh

#
# All these targets must be run regardless of the presence of a file
# named after them.
#
.PHONY: all clean

