#!/usr/bin/env bash
label="$*"
git log --all | sed -ne 's/^commit \([a-f0-9]*\)$/\1/p' | tac | for ((num=0; ; num++))
do
    read commit
    
    logcommit="$commit" #^.."$commit"
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
                    git log -1 --encoding=utf-8 "$logcommit" --pretty=format:%s%n%b
                    echo "$changed_file";
		    echo
                    if [ -n "$object" ]
                    then
                        git cat-file "$type" "$object"
                    fi
	    } | python3 -c 'import charset_normalizer, sys; sys.stdout.write(str(charset_normalizer.from_fp(sys.stdin.buffer).best()))' > ../"$name".file
            }
            git log -1 --patch --follow --find-renames --encoding=utf-8 --pretty=format: "$logcommit" -- "$changed_file" |grep --invert-match --text '^index .*\.\..*' > ../"$name".commit
            subnum=$((subnum+1))
        fi
    done
done
