mkdir -p file_mod
for file in data1d/*
do
    awk '{print NR,$0}' $file|sed 's/[[:blank:]]/,/g'>file_mod/${file##*/}
done