import sys
import transformers

model = 'google/long-t5-tglobal-base'
adapter = 'output/summarization'

config = transformers.AutoConfig.from_pretrained(model)
tokenizer = transformers.AutoTokenizer.from_pretrained(model)
model = transformers.AutoModelForSeq2SeqLM.from_pretrained(model)
model.resize_token_embeddings(len(tokenizer))

adapter_config = transformers.AdapterConfig.load('pfeiffer', non_linearity=None, reduction_factor=None)
model.load_adapter(adapter, config=adapter_config, load_as='summarization')
model.set_active_adapters('summarization')

if __name__ == '__main__':
    model_inputs = tokenizer(sys.stdin.read(), return_tensors='pt')
    all_tokens = [*range(len(tokenizer))]
    def patfn(batch_id, input_ids):
        sys.stdout.write(tokenizer.decode(input_ids[-1]))
        sys.stdout.flush()
        return all_tokens
    output_token_ids = model.generate(**model_inputs, max_new_tokens=4096, prefix_allowed_tokens_fn=patfn)
    #with tokenizer.as_target_tokenizer():
    #    print(tokenizer.decode(output_token_ids[0]))
