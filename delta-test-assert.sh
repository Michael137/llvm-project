#!/bin/bash

SCRIPT_PATH=/Users/michaelbuch/Apple-internal/Radar/subobject-crash

# Return "0" means something interesting happened
# See https://web.archive.org/web/20200701152100/http://delta.tigris.org/
[[ $(${SCRIPT_PATH}/delta-test.sh 2>&1 | grep -c "Assertion failed") -ge 1 ]] && exit 0

exit 1
