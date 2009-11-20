#!/bin/bash
#
# Script to measure the average power drawn by
# the device.
#
# Copyright (C) 2007-2009 Michael Buesch <mb@bu3sch.de>
# GPLv2+
#

PCIBX="./pcibx -V1 $@"	# path to pcibx
cnt=70			# number of measurements


echo "1 + 1" | bc 2>&1 >/dev/null
if [ $? -ne 0 ]; then
	echo "Could not execute \"bc\"."
	echo "Is bc installed?"
	exit 1
fi

function measure # $1=what
{
	local tmp

	tmp="$($PCIBX --cmd-measure$1)"
	[ $? -eq 0 ] || exit 1
	RES="$(echo $tmp | cut -d' ' -f2)"
}

S="scale=3;"
avg_a5="0"
avg_a12="0"
avg_a33="0"
echo -n "measuring" >&2
for ((i = 0; i < $cnt; i++)); do
	echo -n "." >&2
	measure a5
	avg_a5=$(echo "$S $avg_a5 + $RES" | bc)
	measure a12
	avg_a12=$(echo "$S $avg_a12 + $RES" | bc)
	measure a33
	avg_a33=$(echo "$S $avg_a33 + $RES" | bc)
done
echo "OK" >&2
echo >&2
avg_a5=$(echo "$S $avg_a5 / $cnt" | bc)
avg_a12=$(echo "$S $avg_a12 / $cnt" | bc)
avg_a33=$(echo "$S $avg_a33 / $cnt" | bc)
va5=$(echo "$S 5 * $avg_a5" | bc)
va12=$(echo "$S 12 * $avg_a12" | bc)
va33=$(echo "$S 3.3 * $avg_a33" | bc)
va_all=$(echo "$S $va5 + $va12 + $va33" | bc)

echo "average currents:"
echo "5V line:    $avg_a5 Ampere  (= $va5 VA)"
echo "12V line:   $avg_a12 Ampere  (= $va12 VA)"
echo "3.3V line:  $avg_a33 Ampere  (= $va33 VA)"
echo "-------------"
echo "= $va_all VA"

exit 0
