# IFS=$'\n';
# s=" ";
# ln="arr=(\$(find${s}Data_mini/${}-type${}f|sed${}-En${}'s|.*/[^/]+\.([^/.]+)\$|\1|p'|sort${}-u))";
# eval $ln;
# brr=(${arr[@]/#/$*.});
# brr+=("");
# arr+=("Nil");
# for((i=0;i<${#arr[@]};i++));
# do mkdir -p 1cout/${arr[i]};
# ln="find Data_mini/${s}-type${s}f${s}-name${s}\"*${brr[i]}\"${s}-exec${s}mv${s}--target-directory=1cout/${arr[i]}${s}{}${s}+";
# eval $ln;
# done;
# rm -rf Data_mini;
# mv 1cout Data_mini;

V=0;
(($#>1))&&(($2==1))&&V=1;
echo $V