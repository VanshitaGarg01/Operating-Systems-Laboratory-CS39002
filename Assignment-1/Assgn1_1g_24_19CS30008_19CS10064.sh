rm data.csv
for i in {1..150}
do 
    str=$RANDOM
    for j in {1..9}
    do
        str="${str},$RANDOM"
    done
    echo $str >> $1
done

awk -F, '$'$2'~/^[7]/ { c++ } END { if ($c>0) print "YES"; else print "NO" }' $1
