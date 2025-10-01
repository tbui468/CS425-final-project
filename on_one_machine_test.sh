./start_nodes.sh > /dev/null

idx=1
for dir in "$@"; do
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

./stop_nodes.sh > /dev/null

if [ $cnt -eq 1 ]; then
    echo "Passed"
    exit 1
else
    echo "Failed"
    exit 0
fi

