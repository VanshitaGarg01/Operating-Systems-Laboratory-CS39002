echo -n "b=' ';a=\"" > $2
sed -ze 's/\n/;/g' -e 's/\$/\\$/g' -e 's/"/\\"/g' -e 's/ /\${b}/g' $1 >> $2
echo "\";eval \$a" >> $2