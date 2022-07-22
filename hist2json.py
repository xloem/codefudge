#!/usr/bin/env python3

import json, glob
import charset_normalizer

with open("test.json", "wt") as output:
    files = glob.glob('/home/user/src/*.file')
    commits = glob.glob('/home/user/src/*.commit')
    files.sort()
    commits.sort()
    for file, commit in zip(files, commits):
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
