#!/usr/bin/env python3

import json, glob, random
import charset_normalizer

MAX_COMBINED = float('inf')
MAX_INPUT = 1024
MAX_LABEL = float('inf')

with open("test.json", "wt") as output:
    pairs = ((filename, filename[:-len('.file')] + '.commit') for filename in glob.glob('../*.file'))
    for file, commit in pairs:
        assert file[:-len('file')] == commit[:-len('commit')]
        try:
            input = str(charset_normalizer.from_path(file).best())
            label = str(charset_normalizer.from_path(commit).best())
        except:
            continue
        if len(input) + len(label) < MAX_COMBINED and len(input) < MAX_INPUT and len(label) < MAX_LABEL and len(input) > 0 and len(label) > 0:
            print(file, len(input), commit, len(label))
            output.write(json.dumps({
                'input': input,
                'label': label,
            }))
            output.write('\n')
