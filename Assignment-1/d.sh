mkdir files_mod;f=(data1d/*);for((i=0;i<${#f[@]};i++));do awk '{print NR,$0}' ${f[i]}|sed 's/[[:blank:]]/,/g'>files_mod/${f[i]##*/};done