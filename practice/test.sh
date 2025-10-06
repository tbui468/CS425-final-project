#./server 3000 > /dev/null 2>&1 &
#./server 3001 > /dev/null 2>&1 &
#./server 3002 > /dev/null 2>&1 &
#./server 3003 > /dev/null 2>&1 &
#./server 3004 > /dev/null 2>&1 &
#./server 3005 > /dev/null 2>&1 &
#./server 3006 > /dev/null 2>&1 &
#./server 3007 > /dev/null 2>&1 &

./server 3000 &
./server 3001 &
./server 3002 &
./server 3003 &
./server 3004 &
./server 3005 &
./server 3006 &
./server 3007 &

./client 4000

#kill $(lsof -ti :3000)
#./client 4001
#
#kill $(lsof -ti :3001)
#./client 4002

./stop.sh
