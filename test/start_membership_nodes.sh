#!/bin/bash

./stop_nodes.sh > /dev/null 2>&1

pushd ./../
make clean > /dev/null
make > /dev/null
popd

pushd ./temp/machine.3000
./../../../node 3000 &
popd

#sleep to give introducer chance to start up
sleep 1

pushd ./temp/machine.3001
./../../../node 3001 &
popd

pushd ./temp/machine.3002
./../../../node 3002 &
popd

pushd ./temp/machine.3003
./../../../node 3003 &
popd

pushd ./temp/machine.3004
./../../../node 3004 &
popd

pushd ./temp/machine.3005
./../../../node 3005 &
popd

pushd ./temp/machine.3006
./../../../node 3006 &
popd

pushd ./temp/machine.3007
./../../../node 3007 &
popd

#sleep to give nodes to update membership
sleep 1

cnt=`./../dgrep JOIN *.log | grep JOIN | wc -l`
echo "${cnt}"
if [ $cnt -eq 64 ]; then
    echo "Passed"
else
    echo "Failed"
fi

#kill $(lsof -ti :3000)
#kill $(lsof -ti :3001)
#kill $(lsof -ti :3002)
#kill $(lsof -ti :3003)
#kill $(lsof -ti :3004)
#kill $(lsof -ti :3005)
kill $(lsof -ti :3006)
kill $(lsof -ti :3007)
#
##sleep to give nodes to update membership
sleep 1

cnt=`./../dgrep FAIL *.log | grep FAIL | wc -l`
echo "${cnt}"
if [ $cnt -eq 12 ]; then
    echo "Passed"
else
    echo "Failed"
fi
