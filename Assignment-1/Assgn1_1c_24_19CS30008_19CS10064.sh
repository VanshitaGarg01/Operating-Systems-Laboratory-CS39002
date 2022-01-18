dir="1cnew"
# mkdir -p $dir
# ext=""

c=0
for file in $(find data1c/ -mindepth 1 -type f)
do
    # name=$(basename $file)
    # [[ ext=${name##*.} && $ext == $name ]] && ext="Nil"

    # ext=${name##*.}
    # [[ $ext == $name ]] && ext="Nil"
    # arr+=($ext)
    c=$((c+1))

    # if [ $name == $ext ]
    # then
    #     ext="Nil"
    # fi
    # echo $ext

    # mkdir -p $dir/$ext
    # cp $file $dir/$ext/$name
    # cp $file $dir/$name
done

echo $c
# echo ${#arr[@]}