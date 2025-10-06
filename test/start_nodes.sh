#!/bin/bash

pushd ./temp/machine.3000
./../../../node 3000 &
popd

#sleep to give introducer chance to start up
sleep 0.1

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

#sleep to give nodes to start up
sleep 0.1
