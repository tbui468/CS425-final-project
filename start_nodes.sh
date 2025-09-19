#!/bin/bash

pushd ./machine.3000
./../server 3000 &
popd

pushd ./machine.3001
./../server 3001 &
popd

pushd ./machine.3002
./../server 3002 &
popd

pushd ./machine.3003
./../server 3003 &
popd

pushd ./machine.3004
./../server 3004 &
popd

pushd ./machine.3005
./../server 3005 &
popd

pushd ./machine.3006
./../server 3006 &
popd

pushd ./machine.3007
./../server 3007 &
popd
