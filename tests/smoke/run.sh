#!/bin/sh
# run.sh <target> <test dir> [pytest args]: backup, tests, restore however the tests end.
set -u
target=$1
tests=$2
shift 2

make --no-print-directory "$target-backup" || exit 1
python3 -m pytest -c tests/smoke/pytest.ini -s -v "$tests" "$@"
rc=$?
trap '' INT TERM HUP
make --no-print-directory "$target-restore" || rc=1
exit $rc
