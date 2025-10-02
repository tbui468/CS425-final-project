./start_nodes.sh > /dev/null
./stop_nodes.sh > /dev/null

idx=1
for dir in "$@"; do
    pushd ./temp/${dir} > /dev/null
    > ${dir}.log
    cat ./../medium_lorem_ipsum.txt > "${dir}.log"
    popd > /dev/null
    ((idx++))
done

cnt=`./../dgrep Lorem *.log | grep Lorem | wc -l`

#90 occurences of 'Lorem' in each medium_lorem_ipsum.txt file
if [ $cnt -eq 0 ]; then
    echo "Passed"
    exit 1
else
    echo "Failed"
    exit 0
fi
