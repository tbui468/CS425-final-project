for i in {1..100}; do
    ./test.sh > /dev/null
    echo "$i"
done
