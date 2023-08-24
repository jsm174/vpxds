#!/bin/bash

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

Gamepath="${1}"

for ext in "png" "jpg" "jpeg"; do
   path="${Gamepath%.*}-backglass.${ext}"
   if [ -f "${path}" ]; then
      image=$(urlencode "${path}")
      curl -S "http://127.0.0.1:8111/update?display=1&image=${image}" > /dev/null 2>&1
      break
   fi
done

for ext in "png" "jpg" "jpeg"; do
   path="${Gamepath%.*}-dmd.${ext}"
   if [ -f "${path}" ]; then
      image=$(urlencode "${path}")
      curl -S "http://127.0.0.1:8111/update?display=1&image=${image}" > /dev/null 2>&1
      break
   fi
done
