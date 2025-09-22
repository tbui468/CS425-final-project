#!/bin/bash

pushd ./machine.3000
./../node 3000 &
popd

pushd ./machine.3001
./../node 3001 &
popd

pushd ./machine.3002
./../node 3002 &
popd

pushd ./machine.3003
./../node 3003 &
popd

pushd ./machine.3004
./../node 3004 &
popd

pushd ./machine.3005
./../node 3005 &
popd

pushd ./machine.3006
./../node 3006 &
popd

pushd ./machine.3007
./../node 3007 &
popd
