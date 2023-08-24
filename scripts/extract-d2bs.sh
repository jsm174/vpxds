#!/bin/bash

D2BS="$1"

if [ -z "${D2BS}" ]; then
   echo "No file specified."
   exit 1 
fi

if [ ! -f "${D2BS}" ]; then
   echo "${D2BS} not found."
   exit 1
fi

backglass="${D2BS%.*}-backglass.png"
dmd="${D2BS%.*}-dmd.png"

base64_data=$(grep -o 'BackglassImage Value="[^"]*' "$D2BS" | awk -F '"' '{print $2}')
echo "$base64_data" | sed 's/&#xD;//g' | sed 's/&#xA;//g' | base64 --decode > "${backglass}"

base64_data=$(grep -o 'DMDImage Value="[^"]*' "$D2BS" | awk -F '"' '{print $2}')
echo "$base64_data" | sed 's/&#xD;//g' | sed 's/&#xA;//g' | base64 --decode > "${dmd}"

# unfortunately xml tools can't handle large xml nodes

#base64_data=$(xmlstarlet sel -t -v "//DirectB2SData/Images/BackglassImage/@Value" "$D2BS")
#echo "$base64_data" | sed 's/&#13;//g' | base64 --decode > "${backglass}"
#base64_data=$(xmlstarlet sel -t -v "//DirectB2SData/Images/DMDImage/@Value" "$D2BS")
#echo "$base64_data" | sed 's/&#13;//g' | base64 --decode > "${dmd}"
