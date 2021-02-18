#!/bin/bash
#$1 in folder $2 out file 
OUTFILE=$(realpath $2)
touch $OUTFILE
: >> $OUTFILE
printf "FILE,TP,FP,TN,FN\n" >> $OUTFILE
for T in $(seq 0.0 0.01 1.0); do 
    TP=0
    TN=0
    FP=0
    FN=0
    while read -r LINE; do
        LINEARR=(${LINE//,/ })
        POSITIVE=0        
        if [[ ${LINEARR[0]} == *"truck"* || ${LINEARR[0]} == *"Truck"* ]]; then
            POSITIVE=1
        fi
 
        if [[ "${LINEARR[1]}" != "0" ]]; then
            if (( $(echo "${LINEARR[1]} < $T" | bc -l) )); then
                let "TP=$TP+$POSITIVE"
                let "FP=$FP+1-$POSITIVE"
            else
                let "TN=$TN+1-$POSITIVE"
                let "FN=$FN+$POSITIVE"
            fi        
        #ALL
        else 
                let "TN=$TN+1-$POSITIVE"
                let "FN=$FN+$POSITIVE"
        fi
        
    done < "$1"
    printf "$T,$TP,$FP,$TN,$FN\n" >> "$OUTFILE"
done
