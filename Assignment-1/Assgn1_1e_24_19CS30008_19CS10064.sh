V=0
if [[ $2 == "-v" ]]; then
    V=1
fi

function log() {
    if(($V==1)); then
        echo "$@"
    fi
}

REQ_HEADERS=$1

curl https://www.example.com/>example.html
(($?==0))&&log "Fetched example.com"||log "Error fetching example.com"

curl -i http://ip.jsontest.com/
(($?==0))&&log "Fetched IP"||log "Error fetching IP"

arr=(${REQ_HEADERS//,/ })
s=$(printf ".\"%s\", " "${arr[@]}")
s=${s::-2}
curl http://headers.jsontest.com/|jq "$s"
(($?==0))&&log "Fetched headers"||log "Error fetching headers"

for file in JSONData/*
do
    val=$(curl -d "json=`cat $file`" http://validate.jsontest.com/|jq .validate)
    ((${#val}==4))&&(echo ${file##*/}>>valid.txt&&log "$file: valid")||(echo ${file##*/}>>invalid.txt&&log "$file: invalid")
done