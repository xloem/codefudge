#!/usr/bin/env bash
label="$*"
git log --all | sed -ne 's/^commit \([a-f0-9]*\)$/\1/p' | tac | for ((num=0; ; num++))
do
    read commit
    
    logcommit="$commit"^.."$commit"
    subnum=0
    git log --stat "$logcommit" | sed -ne 's/ \([^ ]*\) | \([^ ]*\).*/\1/p' | for ((subnum=0; ; subnum++))
    do
        if ! read changed_file first_word; then break; fi
        if [ "$first_word" != "Bin" ]
        then
            name="$label$(printf %05d "$num")-$(printf %05d "$subnum")"
            git ls-tree "$commit"^ "$changed_file" | {
                read mode type object path
                {
                    echo "$changed_file";
                    if [ -n "$object" ]
                    then
                        git cat-file "$type" "$object"
                    fi
                    git log --encoding=utf-8 --pretty=format:%s%n%b "$logcommit"
                } > ../"$name".file
            }
            git log --patch --encoding=utf-8 --pretty=format: "$logcommit" -- "$changed_file" |grep --invert-match --text '^index .*\.\..*' > ../"$name".commit
            subnum=$((subnum+1))
        fi
    done
done
