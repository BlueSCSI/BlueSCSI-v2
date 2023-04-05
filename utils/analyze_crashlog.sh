#!/bin/bash

# This script can be used for analyzing a crash report.
# It tries to find a matching binary .elf either locally
# or from github releases.
#
# Usage:
#    utils/analyze_crashlog.sh                # paste log to console, press ctrl-D to end
#    utils/analyze_crashlog.sh log.txt        # read log from file
#    utils/analyze_crashlog.sh log.txt path   # read log from file and find firmware at path

if [ "x$1" = "x" ]; then
    logfile=$(mktemp /tmp/crashlog-XXXXXXX)
    cat - > $logfile
else
    logfile=$1
fi

repo="BlueSCSI/BlueSCSI-v2"

# Find firmware compilation time
fwtime=$(grep 'FW Version' $logfile | tail -n 1 | egrep -o '[A-Z][a-z][a-z] [0-9]+ [0-9]+ [0-9:]+')

# Check if the firmware file is available locally
echo "Searching for firmware compiled at $fwtime"
scriptdir=$( dirname -- "${BASH_SOURCE[0]}" )
fwfile=$(find $scriptdir/.. $2 -name '*.elf' -exec grep -q "$fwtime" {} \; -print -quit)

# Search Github for artifacts uploaded within few minutes of the compilation time
if [ "x$fwfile" = "x" ]; then
    echo "Searching on Github"
    enddate=$(date "+%Y-%m-%dT%H:%M" -d "$fwtime 180 seconds")
    runid=$(gh api repos/$repo/actions/artifacts \
        --jq ".artifacts[] | select(.created_at <= \"$enddate\") | .workflow_run.id" | head -n 1)
    if [ "x$runid" != "x" ]; then
        tmpdir=$(mktemp -d /tmp/crashlog-XXXXXXX)
        echo "Workflow run: https://github.com/$repo/actions/runs/$runid"
        echo "Downloading artifact to $tmpdir (if permission denied, use 'gh auth' to login)"
        (cd $tmpdir; gh run download -R $repo $runid)
        fwfile=$(find $tmpdir -name '*.elf' -exec grep -q "$fwtime" {} \; -print -quit)
    fi
fi

if [ "x$fwfile" = "x" ]; then
    echo "Did not find firmware built at $fwtime!"
    exit 1
else
    echo "Found firmware at $fwfile"
fi

# Get crash addresses
echo
for addr in $(egrep -o '[ x][0-9a-fA-Z]{8}' $logfile | tr -d 'x'); do
    arm-none-eabi-addr2line -Cafsip -e "$fwfile" $addr | grep -v ?
done
