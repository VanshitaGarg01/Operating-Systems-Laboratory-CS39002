cat $file|sort -n>1.b.files.out/${file##*/}
cat 1.b.files.out/${file##*/}>>ff.txt