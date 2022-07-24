import difflib, json, transformers, tqdm

tokenizer = transformers.AutoTokenizer.from_pretrained('./fudge-long-t5-tglobal-base/extended_tokenizer')

#import pdb; pdb.set_trace()
#tokenized = tokenizer.encode('a !')
#detokenized = tokenizer.decode(tokenized, clean_up_tokenization_spaces=False)

changes = set()

def accepted_char(chr):
    return ord(chr) < 128 and ord(chr) >= 32

with open('test.json') as datafile:
    filelen=datafile.seek(0,2); datafile.seek(0)
    t = tqdm.tqdm(total=filelen)
    for line in datafile:
        data = json.loads(line)
        vanilla = data['label']
        recoded = tokenizer.decode(tokenizer.encode(vanilla, add_special_tokens = False), clean_up_tokenization_spaces = False)
        if vanilla != recoded:
            diffs = [change for change in  enumerate(difflib.ndiff(vanilla, recoded.replace('<unk>',''))) if change[1][0] != ' ' and accepted_char(change[1][-1]) and accepted_char(vanilla[change[0]-1])]
            if len(diffs):
                changes.update((change for idx, change in diffs))
                print(set(diffs))
                assert not changes
        t.update(len(line))
    assert not changes
        
