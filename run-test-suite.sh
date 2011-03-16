#!/bin/bash

valgrind="valgrind --leak-check=full --quiet --error-exitcode=2"
valgrind=

for f in testsuite/*/*.png; do
    echo -n "${f#testsuite/}: "
    if diff -q <($valgrind ./libpng-dumper $f 2>&1) \
               <($valgrind ./sfpng-dumper $f 2>&1) >/dev/null; then
        echo 'PASS'
    else
        exit=$?
        echo 'FAIL'
        diff -u <($valgrind ./libpng-dumper $f 2>&1) \
                <($valgrind ./sfpng-dumper $f 2>&1)
        exit 1
    fi
done

exit 0
