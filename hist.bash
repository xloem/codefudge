#!/usr/bin/env bash
label="$*"
delim='<pad>'
git log --all | sed -ne 's/^commit \([a-f0-9]*\)$/\1/p' | shuf | for ((num=0; ; num++))
do
    if ! read commit; then break; fi

    rm -rf ../"$label$(printf %05d "$num")-"*
    
    logcommit="$commit"
    git log -1 --stat "$logcommit" | sed -ne 's/ \([^ ]*\) | \([^ ]*\).*/\1/p' | for ((subnum=0; ; subnum++))
    do
        if ! read changed_file first_word rest; then break; fi
        if [ "$first_word" != "Bin" ]
        then
            name="$label$(printf %05d "$num")-$(printf %05d "$subnum")"
            echo "$name $commit $changed_file $first_word $rest"
            git ls-tree "$commit"^ "$changed_file" | {
                read mode type object path
                {
                    git log -1 --encoding=utf-8 "$logcommit" --pretty=format:%s%n%b
                    echo "$delim$changed_file$delim";
		    echo
                    if [ -n "$object" ]
                    then
                        git cat-file "$type" "$object"
                    fi
                } > ../"$name".file
            }
            git log -1 --patch --follow --find-renames --encoding=utf-8 --pretty=format: "$logcommit" -- "$changed_file" |grep --invert-match --text '^index .*\.\..*' > ../"$name".commit
            subnum=$((subnum+1))
        fi
    done
done
