for((i=2,n=$1;i<=n;i++))
do
    while(($n%$i==0))
    do
        arr+=($i)
        n=$(($n/$i))
    done
done
echo ${arr[@]}