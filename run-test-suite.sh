#!/bin/bash

if [ ! -x libpng-dumper -o ! -x sfpng-dumper ]; then
    echo 'run "make check" to build and run the test suite.'
    exit 1
fi

valgrind=
if [ "$1" == "--valgrind" ]; then
    valgrind="valgrind --leak-check=full --quiet --error-exitcode=2"
    shift
fi

sfpng_output=$(mktemp sfpng.XXXXXXXXXX)
libpng_output=$(mktemp libpng.XXXXXXXXXX)
trap "rm -f $sfpng_output $libpng_output" EXIT

inputs=${@:-testsuite/*/*.png}
for f in $inputs; do
    if [ ! -f $f ]; then
        echo "$f: not readable"
        exit 1
    fi

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
