#!/usr/bin/env python3

import json, glob, os, random
import charset_normalizer

MAX_INPUT = 4*65536 # 16GB GPU?
delim = '<pad>'

with open("test.json", "wt") as output:
    commits = set()
    for filename in os.listdir('..'):
        if not filename.endswith('.file') or filename.endswith('.commit'):
            continue
        commit = filename.split('-', 1)[0]
        if commit in commits:
            continue
        commits.add(commit)
        files = glob.glob(os.path.join('..', commit + '-*.file'))
        random.shuffle(files)
        idx = 0
        for filename in files[:4]:
            idx += 1
            diff = filename[:-len('file')] + 'commit'
            try:
                input = str(charset_normalizer.from_path(filename).best() or '')
                label = str(charset_normalizer.from_path(diff).best() or '')
                assert input and label
            except Exception as exc:
                continue
            if len(input) > MAX_INPUT:
                continue
            # add data from other files in the commit
            others = [fn for fn in files if fn != filename]
            random.shuffle(others)
            while len(input) < MAX_INPUT and len(others):
                other = str(charset_normalizer.from_path(others.pop()).best() or '')
                try:
                    other = delim + other.split(delim, 1)[1] # remove commit message
                except:
                    continue
                input += other[:MAX_INPUT-len(input)]
            print(filename, len(input), commit, len(label))
            output.write(json.dumps({
                'input': input,
                'label': label,
            }))
            output.write('\n')
