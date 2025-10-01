./start_nodes.sh > /dev/null
idx=1
for dir in "$@"; do
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

./stop_nodes.sh > /dev/null

if [ $cnt -eq 8 ]; then
    echo "Passed"
    exit 1
else
    echo "Failed"
    exit 0
fi
