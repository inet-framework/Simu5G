#! /bin/bash

FILE1=_instantiatedNedTypesCnt.txt
FILE2=_unusedNedParametersCnt.txt
OUTFILE=_unusedNedParams.txt

#create unused ned parameters list:
DIRECTORY="$SIMU5G_ROOT/tests/fingerprint/results"
PREGEX="^unused parameter: "
find "$DIRECTORY" -type f -name "*.out" | while read -r file; do grep -E "${PREGEX}simu5g\\." "$file"; done \
    | sed "s|$PREGEX||" \
    | awk '{count[$0]++} END {for (line in count) print line, count[line]}' | sort >$FILE2

#create instantiated ned types list:
TREGEX="^instantiated NED type: "
find "$DIRECTORY" -type f -name "*.out" | while read -r file; do grep -E "$TREGEX" "$file"; done \
    | sed "s|$TREGEX||" | grep "^simu5g\\." \
    | awk '{count[$0]++} END {for (line in count) print line, count[line]}' | sort >$FILE1

# Search and comparison
while IFS=' ' read -r path count; do
    # Generate the shortened path
    short_path=$(echo "$path" | sed 's/\.[^.]*$//')

    # Check if the shortened path exists in the first file
    if ! grep -q "^$short_path " "$FILE1"; then
        echo "$path - $count - UNUSED - Module not found"
    else
        # If found, compare the count
        file1_count=$(grep "^$short_path " "$FILE1" | awk '{print $2}')
        if [ "$file1_count" == "$count" ]; then
            echo "$path - $count - UNUSED"
        else
            difference=$((file1_count - count))
            echo "$path - $difference:$count/$file1_count - SEMIUSED"
        fi
    fi
done < "$FILE2" >$OUTFILE
