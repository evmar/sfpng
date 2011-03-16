#!/bin/bash

for f in testsuite/*.png; do
    echo -n "$f: "
    if diff -q <(./libpng-dumper $f 2>&1) <(./sfpng-dumper $f) >/dev/null; then
        echo 'PASS'
    else
        echo 'FAIL'
        diff -u <(./libpng-dumper $f 2>&1) <(./sfpng-dumper $f)
        exit 1
    fi
done

exit 0
