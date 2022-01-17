n=$1
for((i=2;i<=n;i++))
do
    while(($n%$i==0))
    do
        arr+=($i)
        n=$(($n/$i))
    done
done
echo ${arr[@]}