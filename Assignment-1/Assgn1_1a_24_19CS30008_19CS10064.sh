n=$1
for((i=2;i*i<=n;i++))
do
    while(($n%$i==0))
    do
        arr+=($i)
        n=$(($n/$i))
    done
done
if(($n>1))
then
    arr+=($n)
fi
echo ${arr[@]}