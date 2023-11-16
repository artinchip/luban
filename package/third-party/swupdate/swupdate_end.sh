#!/bin/sh

swu_status=$(fw_printenv -n swu_status 2>/dev/null)

[ x"$swu_status" = x"" ] && {
    echo "No OTA upgrade information found"
	return
}

if [ x"$swu_status" = "xfinish" ]; then
	fw_setenv swu_status
    echo "Last OTA upgrade succeeded"
fi

