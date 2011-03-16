#!/bin/bash

for f in testsuite/*.png; do
    echo -n "$f: "
    diff -q <(./libpng-dumper $f) <(./sfpng-dumper $f) >/dev/null
    if [ $? ]; then
        echo 'FAIL'
        diff -u <(./libpng-dumper $f) <(./sfpng-dumper $f)
        exit 1
    fi
    echo 'PASS'
done

exit 0
