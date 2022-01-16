mkdir -p file_mod
for file in data1d/temp/*
do
    # awk '{print NR,$0}' $file|sed 's/ /,/g'>file_mod/$(basename $file)  # can replace space with [[:blank:]] but that takes double time
    awk '{print NR,$0}' $file|tr '[[:blank:]]' ','>file_mod/$(basename $file)
done
