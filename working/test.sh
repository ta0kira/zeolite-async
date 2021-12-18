#!/usr/bin/env bash

delay=$1
message=$2
status=$3

sleep "$delay"
echo "$message"
exit ${status-0}
