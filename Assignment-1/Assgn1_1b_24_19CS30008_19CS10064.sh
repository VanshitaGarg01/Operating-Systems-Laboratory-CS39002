mkdir -p 1.b.files.out
for file in 1.b.files/*.txt
do
    echo ${file##*/}
    cat $file|sort -n>1.b.files.out/${file##*/}>>ff.txt
done
sort -n ff.txt|uniq -c|awk '{print $2,$1}'>1.b.out.txt
rm ff.txt