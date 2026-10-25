#!/bin/sh
# usage: test.sh <plain-backend>
set -u
backend=$1
dir=$(dirname "$0")
code=0

expect() {
	name=$1
	expected=$2
	actual=$3
	if [ "$expected" = "$actual" ]; then
		echo "$name: OK"
	else
		echo "$name: FAIL"
		echo "expected:"
		echo "$expected"
		echo "actual:"
		echo "$actual"
		code=1
	fi
}

all=$("$backend" --caps all "$dir/caps.sv" --top top 2>/dev/null)
expect "all capabilities" "module top
memory mem 2
tristate 1
blackbox BOX A:in:1 B:in:1:open Y:out:1 P:inout:1:open
module sub\$top.u
instance sub\$top.u y:inout:1 r:out:1
latch 1
print \$display
check assert" "$all"

none=$("$backend" "$dir/caps.sv" --top top -DNO_CHECKS 2>/dev/null)
expect "no capabilities" "module top
blackbox BOX A:in:1 Y:out:1
module sub\$top.u
instance sub\$top.u y:inout:1 r:out:1" "$none"

for call in add_tristate:tristates add_latch:latches add_check:checks add_print:prints; do
	name=${call%%:*}
	cap=${call##*:}
	define=-DNO_CHECKS
	[ "$cap" = checks ] && define=-DWITH_CHECKS
	out=$("$backend" --caps "$cap" --default "$name" "$dir/caps.sv" --top top $define 2>/dev/null)
	status=$?
	case "$out" in
	*"internal error"*"Capabilities::$cap overrides $name"*) [ $status -eq 2 ] && echo "$name default: OK" && continue ;;
	esac
	echo "$name default: FAIL (status $status)"
	echo "$out"
	code=1
done

exit $code
