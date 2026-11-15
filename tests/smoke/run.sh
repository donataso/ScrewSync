#!/bin/bash
# run.sh <target> <test dir> [pytest args]: backup, tests, restore however the tests end.
set -u
trap : INT TERM HUP
target=$1
tests=$2
shift 2

log=/tmp/screwsync-smoke-$target-$(date +%Y%m%d-%H%M%S).log
# tee ignores the signals so the restore output is still logged after Ctrl-C.
exec > >(trap '' INT TERM HUP; exec tee "$log") 2>&1
tee_pid=$!
export PYTHONUNBUFFERED=1

make --no-print-directory "$target-backup" || rc=1
if [ -z "${rc:-}" ]; then
    python3 -m pytest -c tests/smoke/pytest.ini -s -v "$tests" "$@"
    rc=$?
    trap '' INT TERM HUP
    make --no-print-directory "$target-restore" || rc=1
fi
echo "log: $log"
exec >&- 2>&-
wait "$tee_pid"
exit $rc
