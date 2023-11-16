#!/bin/sh
#Copyright (c) 2022-2024 luban-dev Technology Co. Ltd.

# ============================================================================
# GLOBAL FUNCTIONS
# ============================================================================
swupdate_cmd()
{
	while true
	do
		swu_param=$(fw_printenv -n swu_param 2>/dev/null)
		swu_boardname=$(fw_printenv -n swu_boardname 2>/dev/null)
		swu_entry=$(fw_printenv -n swu_entry 2>/dev/null)
		swu_version=$(fw_printenv -n swu_version 2>/dev/null)
		echo "swu_param: ##$swu_param##"
		echo "swu_boardname: ##$swu_boardname##"
		echo "swu_entry: ##$swu_entry##"

		check_version_para=""
		[ x"$swu_version" != x"" ] && {
			echo "now version is $swu_version"
			check_version_para="-N $swu_version"
		}

		[ x"$swu_entry" = x"" ] && {
			echo "no swupdate_cmd to run, wait for reboot swupdate"
			return
		}

		echo "###now do swupdate###"

		echo "##swupdate -v$swu_param -e "$swu_boardname,$swu_entry" ##"
		swupdate -v$swu_param -e "$swu_boardname,$swu_entry"

		swu_reboot=$(fw_printenv -n swu_reboot 2>/dev/null)
		echo "swu_reboot: ##$swu_reboot##"
		if [ x"$swu_reboot" = "xyes" ]; then
			fw_setenv swu_reboot
			reboot -f
		fi

		sleep 1
	done
}

# ============================================================================
# MAIN
# ============================================================================
mkdir -p /var/lock

[ $# -ne 0 ] && {
	echo "config new swupdate"
	swu_input=$*
	echo "swu_input: ##$swu_input##"

	swu_param=$(echo " $swu_input" | sed -E 's/ -e +[^ ]*//')
#	echo "swu_param: ##$swu_param##"
	echo "swu_param=$swu_param" > /tmp/swupdate_param_file
	swu_param_e=$(echo " $swu_input" | awk -F ' -e ' '{print $2}')
	swu_param_e=$(echo "$swu_param_e" | awk -F ' ' '{print $1}')
	swu_boardname=$(echo "$swu_param_e" | awk -F ',' '{print $1}')
#	echo "swu_boardname: ##$swu_boardname##"
	echo "swu_boardname=$swu_boardname" >> /tmp/swupdate_param_file
	swu_entry=$(echo "$swu_param_e" | awk -F ',' '{print $2}')
#	echo "swu_entry: ##$swu_entry##"
	echo "swu_entry=$swu_entry" >> /tmp/swupdate_param_file
	fw_setenv -s /tmp/swupdate_param_file
	sync

	echo "## set swupdate_param done ##"

}

swupdate_cmd
