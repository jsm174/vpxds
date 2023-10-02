#!/bin/bash

Gamepath="${2}"

urlencode() {
  local string="${1}"
  local strlen=${#string}
  local encoded=""
  local pos c o

  for (( pos=0 ; pos<strlen ; pos++ )); do
     c=${string:$pos:1}
     case "$c" in
        [-_.~a-zA-Z0-9] ) o="${c}" ;;
        * )               printf -v o '%%%02x' "'$c"
     esac
     encoded+="${o}"
  done
  echo "${encoded}"
}

vpx=$(urlencode "${Gamepath}")

curl -S "http://127.0.0.1:8111/b2s?vpx=${vpx}" > /dev/null 2>&1