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

for ext in "png" "jpg" "jpeg"; do
   path="${Gamepath%.*}-backglass.${ext}"
   if [ -f "${path}" ]; then
      image=$(urlencode "${path}")
      curl -S "http://127.0.0.1:8111/update?display=backglass&image=${image}" > /dev/null 2>&1
      break
   fi
done

for ext in "png" "jpg" "jpeg"; do
   path="${Gamepath%.*}-dmd.${ext}"
   if [ -f "${path}" ]; then
      image=$(urlencode "${path}")
      curl -S "http://127.0.0.1:8111/update?display=dmd&image=${image}" > /dev/null 2>&1
      break
   fi
done
