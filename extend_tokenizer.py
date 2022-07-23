import transformers
import json, os, pickle

models = ['google/long-t5-tglobal-base','xlnet-base-cased','transfo-xl-wt103']
tokens = ['\n']

for model in models:
    model_name = 'fudge-' + model.split('/')[-1]

    tokenizer = transformers.AutoTokenizer.from_pretrained(model)
    def recodes(token):
        ids = tokenizer.encode(token, add_special_tokens=False)
        return tokenizer.decode(ids) == token

    extra_tokens = [token for token in tokens if not recodes(token)]
    if len(extra_tokens):
        output_path = os.path.join(model_name, 'extended_tokenizer')

        tokenizer_paths = tokenizer.save_pretrained('tmp')

        #import pdb; pdb.set_trace()
        for tokenizer_file_path in tokenizer_paths:
            try:
                with open(tokenizer_file_path) as tokenizer_file:
                    vocab = json.load(tokenizer_file)
                format = 'json'
            except:
                try:
                    with open(tokenizer_file_path, 'rb') as tokenizer_file:
                        vocab = pickle.load(tokenizer_file)
                    format = 'pickle'
                    break # not sure how to properly replace a token in these
                except:
                    continue

            if 'model' in vocab and 'vocab' in vocab['model']:
                replaces = vocab['model']['vocab'][-len(extra_tokens):]
                vocab['model']['vocab'][-len(extra_tokens):] = [[extra_token, *rest] for extra_token, (replace, *rest) in zip(extra_tokens, replaces)]
            if 'added_tokens' in vocab:
                replaces = vocab['added_tokens'][-len(extra_tokens):]
                vocab['added_tokens'][-len(extra_tokens):] = [{**rest, 'content': extra_token} for extra_token, rest in zip(extra_tokens, replaces)]
            if 'additional_special_tokens' in vocab:
                vocab['additional_special_tokens'][:len(extra_tokens)] = []
            if 'extra_ids' in vocab:
                vocab['extra_ids'] -= len(extra_tokens)
            if 'idx2sym' in vocab and 'sym2idx' in vocab:
                replaces = vocab['idx2sym'][-len(extra_tokens):]
                vocab['idx2sym'][-len(extra_tokens):] = extra_tokens
                for new, old in zip(extra_tokens, replaces):
                    idx = vocab['sym2idx'].pop(old)
                    vocab['sym2idx'][new] = idx

            if format == 'json':
                with open(tokenizer_file_path, 'wt') as tokenizer_file:
                    json.dump(vocab, tokenizer_file)
            #elif format == 'pickle':
            #    with open(tokenizer_file_path, 'wb') as tokenizer_file:
            #        pickle.dump(vocab, tokenizer_file)
        tokenizer = transformers.AutoTokenizer.from_pretrained('tmp')
        tokenizer.save_pretrained(output_path)
        if format != 'pickle':
            for token in tokens:
                assert recodes(token)
            print('extended tokenizer saved to', output_path)
        else:
            print(model, 'uses a pickle tokenizer format that is not supported yet, but was saved to', output_path)
    else:
        print(model, 'already has the requested tokens')
