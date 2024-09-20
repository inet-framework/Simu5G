#!/bin/bash

#create all ned types list:
SDIRECTORY=$SIMU5G_ROOT/src
find "$SDIRECTORY" -type f -name '*.ned' | sed "s|^$SDIRECTORY/|simu5g.|" \
    | sed "s|\.ned\$||" | sed "s|/|.|g" | sort >_allNedTypes.txt

#create instantiated ned types list:
RDIRECTORY="$SIMU5G_ROOT/tests/fingerprint/results"
RREGEX="^instantiated NED type: "
find "$RDIRECTORY" -type f -name "*.out" | while read -r file; do grep -E "$RREGEX" "$file"; done \
    | grep "simu5g." | sort -u | sed "s|$RREGEX||" >_instantiatedNedTypes.txt

#create unused ned types list:
grep -Fxv -f _instantiatedNedTypes.txt _allNedTypes.txt | grep -v -E "Base\$"  | grep -v -E "\\.I[A-Z]\\w+\$" | sort >_unusedNedTypes.txt
