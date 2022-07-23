#!/usr/bin/env python3

import json, glob, random
import charset_normalizer

with open("test.json", "wt") as output:
    files = glob.glob('../*.file')
    commits = glob.glob('../*.commit')
    files.sort()
    commits.sort()
    pairs = [*zip(files,commits)]
    #random.shuffle(pairs)
    for file, commit in pairs:
        print(file, commit)
        assert file[:-len('file')] == commit[:-len('commit')]
        input = str(charset_normalizer.from_path(file).best())
        label = str(charset_normalizer.from_path(commit).best())
        if len(input) + len(label) < 2048 and len(input) > 0 and len(label) > 0:
            print(file, len(input), commit, len(label))
            output.write(json.dumps({
                'input': input,
                'label': label,
            }))
            output.write('\n')
