import transformers
import os

models = ['google/long-t5-tglobal-base','xlnet-base-cased','transfo-xl-wt103']
tokens = ['\n']

for model in models:
    model_name = 'fudge-' + model.split('/')[-1]

    vanilla_tokenizer = transformers.AutoTokenizer.from_pretrained(model)
    def recodes(token):
        ids = vanilla_tokenizer.encode(token, add_special_tokens=False)
        return vanilla_tokenizer.decode(ids) == token

    extra_tokens = [token for token in tokens if not recodes(token)]
    if len(extra_tokens):
        vanilla_tokenizer.add_tokens(extra_tokens)
        for token in tokens:
            assert recodes(token)
        output_path = os.path.join(model_name, 'extended_tokenizer')
        vanilla_tokenizer.save_pretrained(output_path)
        print('extended tokenizer saved to', output_path)
    else:
        print(model, 'already has the requested tokens')
