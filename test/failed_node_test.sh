./start_some_nodes.sh > /dev/null

idx=1
for dir in "$@"; do
    pushd ./temp/${dir} > /dev/null
    > ${dir}.log
    cat ./../medium_lorem_ipsum.txt > "${dir}.log"
    popd > /dev/null
    ((idx++))
done


cnt=`./../dgrep Lorem *.log | grep Lorem | wc -l`

kill $(lsof -ti :3000)
kill $(lsof -ti :3002)
kill $(lsof -ti :3003)
kill $(lsof -ti :3004)
kill $(lsof -ti :3005)
kill $(lsof -ti :3006)
kill $(lsof -ti :3007)
#./stop_nodes.sh > /dev/null

#90 occurences of 'Lorem' in each medium_lorem_ipsum.txt file
if [ $cnt -eq 630 ]; then
    echo "Passed"
    exit 1
else
    echo "Failed"
    exit 0
fi
