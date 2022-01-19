IFS=$'\n'
arr=($(find Data_mini/ -type f -name "*.*" | awk -F. '{print $NF}' | sort -u))
echo ${arr[@]}
for ext in ${arr[@]}
do
    echo $ext
    mkdir -p 1cout/$ext
    find Data_mini/ -type f -name "*.$ext" -exec mv -t ./1cout/"$ext" {} +
done
mkdir -p 1cout/Nil
find Data_mini/ -type f -exec mv -t ./1cout/Nil {} +