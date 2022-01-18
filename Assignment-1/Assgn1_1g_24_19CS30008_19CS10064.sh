for((i=0;i<150;i++))
do 
    for((j=0,str=$RANDOM;j<9;j++))
    do
        str="${str},$RANDOM"
    done
    echo $str>>$1
done
awk -F, '$'$2'~/'$3'/{c++}END{print(c>0)?"YES":"NO"}' $1