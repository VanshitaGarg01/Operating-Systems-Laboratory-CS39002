mkdir -p 1.b.files.out
for file in 1.b.files/*.txt
do
    cat $file|sort -n>1.b.files.out/${file##*/}
done
sort -nm 1.b.files.out/*|uniq -c|awk '{print $2,$1}'>1.b.out1.txt