#!/bin/bash

#setup
./stop_nodes.sh > /dev/null 2>&1

pushd ./../
make clean > /dev/null
make > /dev/null
popd

> ./temp/medium_lorem_ipsum.txt
for i in {1..10}; do
    cat lorem_ipsum.txt >> ./temp/medium_lorem_ipsum.txt
done

> ./temp/large_lorem_ipsum.txt
for i in {1..10}; do
    cat ./temp/medium_lorem_ipsum.txt >> ./temp/large_lorem_ipsum.txt
done

total=0
passed=0

machine_dirs=("machine.3000" 
      "machine.3001"
      "machine.3002"
      "machine.3003"
      "machine.3004"
      "machine.3005"
      "machine.3006"
      "machine.3007")

echo "========Basic Test========"
./basic_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========On One Machine Test========"
./on_one_machine_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========On Some Machines Test========"
./on_some_machines_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========On All Machines Test========"
./on_all_machines_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========Medium-Sized Log Test========"
./medium_sized_log_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========Large-Sized Log Test========"
./large_sized_log_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========Medium-Sized Result Test========"
./medium_sized_result_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========Large-Sized Result Test========"
./large_sized_result_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========Failed Node Test========"
./failed_node_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))

echo "========Failed Nodes Test========"
./failed_nodes_test.sh ${machine_dirs[@]}
passed=$((passed + $?))
((total++))


echo "========TOTAL PASSED========"
echo "${passed}/${total}"


