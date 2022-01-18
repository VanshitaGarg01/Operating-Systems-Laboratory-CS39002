for((i=1;i<=150*10;i++))
do 
    ((i%10==0))&&printf "$RANDOM\n">>$1||printf "$RANDOM,">>$1
done
awk -F, '$'$2'~/'$3'/{c++}END{print(c>0)?"YES":"NO"}' $1