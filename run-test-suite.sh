#!/bin/bash

valgrind="valgrind --leak-check=full --quiet --error-exitcode=2"
valgrind=

sfpng_output=$(mktemp sfpng.XXXXXXXXXX)
libpng_output=$(mktemp libpng.XXXXXXXXXX)
trap "rm -f $sfpng_output $libpng_output" EXIT

for f in testsuite/*/*.png; do
    echo -n "$f: "
    $valgrind ./libpng-dumper $f 2>&1 > $libpng_output
    $valgrind ./sfpng-dumper $f 2>&1 > $sfpng_output

    if diff -q $libpng_output $sfpng_output; then
        echo 'PASS'
    else
        exit=$?
        echo 'FAIL'
        diff -U5 $libpng_output $sfpng_output
        exit 1
    fi
done

exit 0
