import transformers
import json, os, pickle

models = ['google/long-t5-tglobal-base','xlnet-base-cased','transfo-xl-wt103']
tokens = ['\n', ' ', '{', '}']

for model in models:
    model_name = 'fudge-' + model.split('/')[-1]

    tokenizer = transformers.AutoTokenizer.from_pretrained(model)
    def recodes(token):
        ids = tokenizer.encode(token, add_special_tokens=False)
        return tokenizer.decode(ids) == token

    extra_tokens = [token for token in tokens if not recodes(token)]
    output_path = os.path.join(model_name, 'extended_tokenizer')

    tokenizer_paths = tokenizer.save_pretrained('tmp')

    for tokenizer_file_path in tokenizer_paths:
        try:
            with open(tokenizer_file_path) as tokenizer_file:
                config = json.load(tokenizer_file)
            format = 'json'
        except:
            try:
                with open(tokenizer_file_path, 'rb') as tokenizer_file:
                    config = pickle.load(tokenizer_file)
                format = 'pickle'
                break # not sure how to properly replace a token in these
            except:
                continue

        if 'pre_tokenizer' in config and 'pretokenizers' in config['pre_tokenizer']:
            pretokenizers = config['pre_tokenizer']['pretokenizers']
            for pretokenizer in pretokenizers:
                if pretokenizer.get('add_prefix_space'):
                    pretokenizer['add_prefix_space'] = False
        if 'decoder' in config and config['decoder'].get('add_prefix_space'):
            config['decoder']['add_prefix_space'] = False

        if 'model' in config and 'vocab' in config['model']:
            replaces = config['model']['vocab'][-len(extra_tokens):]
            config['model']['vocab'][-len(extra_tokens):] = [[extra_token, *rest] for extra_token, (replace, *rest) in zip(extra_tokens, replaces)]
        if 'added_tokens' in config:
            replaces = config['added_tokens'][-len(extra_tokens):]
            config['added_tokens'][-len(extra_tokens):] = [{**rest, 'content': extra_token} for extra_token, rest in zip(extra_tokens, replaces)]
        if 'additional_special_tokens' in config:
            config['additional_special_tokens'][:len(extra_tokens)] = []
        if 'extra_ids' in config:
            config['extra_ids'] -= len(extra_tokens)
        if 'idx2sym' in config and 'sym2idx' in config:
            replaces = config['idx2sym'][-len(extra_tokens):]
            config['idx2sym'][-len(extra_tokens):] = extra_tokens
            for new, old in zip(extra_tokens, replaces):
                idx = config['sym2idx'].pop(old)
                config['sym2idx'][new] = idx

        if format == 'json':
            with open(tokenizer_file_path, 'wt') as tokenizer_file:
                json.dump(config, tokenizer_file)
        #elif format == 'pickle':
        #    with open(tokenizer_file_path, 'wb') as tokenizer_file:
        #        pickle.dump(config, tokenizer_file)
    tokenizer = transformers.AutoTokenizer.from_pretrained('tmp')
    tokenizer.save_pretrained(output_path)
    if format != 'pickle':
        for token in tokens:
            assert recodes(token)
        print('extended tokenizer saved to', output_path)
    else:
        print(model, 'uses a pickle tokenizer format that is not supported yet, but was saved to', output_path)
