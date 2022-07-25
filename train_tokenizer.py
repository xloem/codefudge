#import sentencepiece as spm
import tokenizers, transformers
import charset_normalizer
import glob

model = 'google/long-t5-tglobal-base'
model_name = model.split('/')[-1]

vanilla_tokenizer = transformers.AutoTokenizer.from_pretrained(model)

#def none2neg1(val):
#    return val if val is not None else -1

print('Listing files ...', end='', flush=True)
files = glob.glob('../*.file') + glob.glob('../*.commit')
print('Done')

def input_text():
    for file in files:
        yield str(charset_normalizer.from_path(file).best())
#
#spm.SentencePieceTrainer.train(
#    sentence_iterator=input_text(),
#    model_prefix=model_name,
#    max_sentence_length=65536,
#    vocab_size=vanilla_tokenizer.vocab_size - len(vanilla_tokenizer.additional_special_tokens_ids),
#    unk_id=none2neg1(vanilla_tokenizer.unk_token_id),
#    bos_id=none2neg1(vanilla_tokenizer.bos_token_id),
#    eos_id=none2neg1(vanilla_tokenizer.eos_token_id),
#    pad_id=none2neg1(vanilla_tokenizer.pad_token_id),
#    model_type="unigram",
#    train_extremely_large_corpus=True,
#) 
import tokenizers
tokenizer = tokenizers.Tokenizer(tokenizers.models.Unigram())
trainer = tokenizers.trainers.UnigramTrainer(
        vocab_size=vanilla_tokenizer.vocab_size - len(vanilla_tokenizer.additional_special_tokens_ids),
        special_tokens=['<pad>', '<bom>', '<eom>', '<bofn>', '<eofn>', '<bof>', '<eof>'],
        max_piece_length=1024,
)
words = tokenizers.Regex('[A-Za-z_][0-9A-Za-z_]*| {2}| {4}| {8}|.')
tokenizer.pre_tokenizer = tokenizers.pre_tokenizers.Split(words, 'isolated')
tokenizer.train_from_iterator(input_text(), trainer=trainer, length=len(files))
tokenizer.save(f'{model_name}/trained_tokenizer.json')
