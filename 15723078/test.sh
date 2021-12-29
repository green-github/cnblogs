#!/bin/bash

for i in {1..10}
do
    ./a.out 1>/dev/null 2>>$1
    sleep 10
done

