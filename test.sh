#!/bin/bash

#setup
./stop_nodes.sh > /dev/null 2>&1
make clean > /dev/null
make > /dev/null

> medium_lorem_ipsum.txt
for i in {1..10}; do
    cat lorem_ipsum.txt >> medium_lorem_ipsum.txt
done

> large_lorem_ipsum.txt
for i in {1..100}; do
    cat medium_lorem_ipsum.txt >> large_lorem_ipsum.txt
done

total=0
passed=0

dirs=("machine.3000" 
      "machine.3001"
      "machine.3002"
      "machine.3003"
      "machine.3004"
      "machine.3005"
      "machine.3006"
      "machine.3007")

echo "========Basic Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    for i in {1..10}; do
        if [ $idx -eq $i ]; then
            echo "dog" >> "${dir}.log"
        else
            echo "cat" >> "${dir}.log"
        fi
    done
    popd > /dev/null
    ((idx++))
done

#./dgrep -E "\"Listening.*local|local.*Listening\"" *.log
cnt=`./dgrep dog *.log | grep dog | wc -l`
if [ $cnt -eq 8 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi

./stop_nodes.sh > /dev/null

echo "========On One Machine Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    if [ $idx -eq 4 ]; then
        echo "dog" >> "${dir}.log"
    fi
    for i in {1..10}; do
        echo "cat" >> "${dir}.log"
    done
    popd > /dev/null
    ((idx++))
done

cnt=`./dgrep dog *.log | grep dog | wc -l`
if [ $cnt -eq 1 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi

./stop_nodes.sh > /dev/null

echo "========On Some Machines Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    if [ $idx -lt 5 ]; then
        echo "dog" >> "${dir}.log"
    fi
    for i in {1..10}; do
        echo "cat" >> "${dir}.log"
    done
    popd > /dev/null
    ((idx++))
done

cnt=`./dgrep dog *.log | grep dog | wc -l`
if [ $cnt -eq 4 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi

./stop_nodes.sh > /dev/null

echo "========On All Machines Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    for i in {1..10}; do
        echo "cat" >> "${dir}.log"
    done
    echo "dog" >> "${dir}.log"
    popd > /dev/null
    ((idx++))
done

cnt=`./dgrep dog *.log | grep dog | wc -l`
if [ $cnt -eq 8 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi

./stop_nodes.sh > /dev/null

echo "========Medium-Sized Log Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    cat ./../medium_lorem_ipsum.txt > "${dir}.log"
    for i in {1..10}; do
        echo "cat" >> "${dir}.log"
    done
    echo "dog" >> "${dir}.log"
    popd > /dev/null
    ((idx++))
done

cnt=`./dgrep dog *.log | grep dog | wc -l`
if [ $cnt -eq 8 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi

./stop_nodes.sh > /dev/null

echo "========Large-Sized Log Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    cat ./../large_lorem_ipsum.txt > "${dir}.log"
    for i in {1..10}; do
        echo "cat" >> "${dir}.log"
    done
    echo "dog" >> "${dir}.log"
    popd > /dev/null
    ((idx++))
done

cnt=`./dgrep dog *.log | grep dog | wc -l`
if [ $cnt -eq 8 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi

./stop_nodes.sh > /dev/null

echo "========Medium-Sized Result Test========"
((total++))

./start_nodes.sh > /dev/null

idx=1
for dir in "${dirs[@]}"; do
    pushd ./${dir} > /dev/null
    > ${dir}.log
    cat ./../medium_lorem_ipsum.txt > "${dir}.log"
    popd > /dev/null
    ((idx++))
done

cnt=`./dgrep Lorem *.log | grep Lorem | wc -l`
if [ $cnt -eq 8 ]; then
    echo "Passed"
    ((passed++))
else
    echo "Failed"
fi
echo $cnt
./stop_nodes.sh > /dev/null

echo "========Large-Sized Result Test========"
((total++))
echo "Failed"

echo "========Failed Node Test========"
((total++))
echo "Failed"

echo " "
echo "========TOTAL PASSED========"
echo "${passed}/${total}"
