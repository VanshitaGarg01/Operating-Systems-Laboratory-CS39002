# IFS=$'\n';arr=($(find Data_mini/ -type f -name "*.*"|awk -F. '{print $NF}'|sort -u));
# echo ${arr[@]}
# echo ${#arr[@]}
# brr=(${arr[@]/#/\*.});
# arr+=("Nil");
# brr+=("z\*");
# echo ${#brr[@]}
# echo "${brr[@]}";
# for((i=0;i<${#arr[@]};i++));
# 	do mkdir -p 1cout/${arr[i]};
# 	echo ${brr[i]}
# 	brr[i]=${brr[i]:1}
# 	# echo ${brr[i]}
# 	find Data_mini/ -type f -name "'${brr[i]}'" -exec mv -t 1cout/${arr[i]} {} +;
# done;
# mkdir -p 1cout/Nil;
# find Data_mini/ -type f -exec mv -t ./1cout/Nil {} +


IFS=$'\n'
arr=($(find Data_mini/ -type f -name "*.*"|awk -F. '{print $NF}'|sort -u))
for ext in ${arr[@]}
do
	mkdir -p 1cout/$ext
	find Data_mini/ -type f -name "*.$ext" -exec mv --target-directory=1cout/"$ext" {} +
done
mkdir -p 1cout/Nil
find Data_mini/ -type f -exec mv --target-directory=1cout/Nil {} +