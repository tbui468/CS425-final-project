
./start_nodes.sh > /dev/null

idx=1
for dir in "$@"; do
    pushd ./temp/${dir} > /dev/null
    > ${dir}.log
    cat ./../large_lorem_ipsum.txt > "${dir}.log"
    for i in {1..10}; do
        echo "cat" >> "${dir}.log"
    done
    echo "dog" >> "${dir}.log"
    popd > /dev/null
    ((idx++))
done

cnt=`./../dgrep dog *.log | grep dog | wc -l`
./stop_nodes.sh > /dev/null

if [ $cnt -eq 8 ]; then
    echo "Passed"
    exit 1
else
    echo "Failed"
    exit 0
fi

