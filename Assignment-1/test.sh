file="test.txt"
awk 'NR{$0=++a " " $0};{gsub(/[[:blank:]]/,",")};{print}' $file