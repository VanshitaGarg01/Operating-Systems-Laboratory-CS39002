# Q1
REQ_HEADERS="Accept,Host,User-Agent"

# Q2
# curl https://www.example.com/>example.html

# Q3
# curl -i http://ip.jsontest.com/

# Q4
# arr=(${REQ_HEADERS//,/ })
# s=$(printf ".\"%s\", " "${arr[@]}")
# s=${s::-2}
# curl http://headers.jsontest.com/|jq "$s"

# Q5
# for file in JSONData/*
# do
#     val=$(curl -d "json=`cat $file`" http://validate.jsontest.com/|jq .validate)
#     [[ $val == "true" ]]&&echo ${file##*/}>>valid.txt||echo ${file##*/}>>invalid.txt
# done