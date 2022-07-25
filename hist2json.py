#!/usr/bin/env python3

import json, glob, os, random
import charset_normalizer

MAX_INPUT = 16*65536 # not seeming to run into a bound to this on a 16GB GPU (8x64k worked fine); maybe it is trimmed elsewhere, or very large
#MAX_INPUT = 320 # 2GB GPU RAM

MAX_FILES_PER_COMMIT=4
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
        for filename in files:
            if idx >= MAX_FILES_PER_COMMIT:
                break
            diff = filename[:-len('file')] + 'commit'
            try:
                input = str(charset_normalizer.from_path(filename).best() or '') + delim
                label = str(charset_normalizer.from_path(diff).best() or '') + delim
                assert input and label
            except Exception as exc:
                continue
            if len(input) > MAX_INPUT:
                continue
            idx += 1
            # add data from other files in the commit
            others = [fn for fn in files if fn != filename]
            random.shuffle(others)
            while len(input) < MAX_INPUT and len(others):
                other = str(charset_normalizer.from_path(others.pop()).best() or '')
                try:
                    other = delim + other.split(delim, 1)[1] + delim # remove commit message
                except:
                    continue
                input += other[:MAX_INPUT-len(input)]
            print(filename, len(input), commit, len(label))
            output.write(json.dumps({
                'input': input,
                'label': label + '</s>',
            }))
            output.write('\n')
