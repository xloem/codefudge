import sentencepiece as spm
import transformers
import charset_normalizer
import glob

model = 'google/long-t5-tglobal-base'
model_name = model.split('/')[-1]

vanilla_tokenizer = transformers.AutoTokenizer.from_pretrained(model)

# the existing tokenizer has 100 extra tokens at the end that could possibly be used instead of training a new one

def none2neg1(val):
    return val if val is not None else -1

def input_text():
    for file in glob.glob('../*.file') + glob.glob('../*.commit'):
        yield str(charset_normalizer.from_path(file).best())

spm.SentencePieceTrainer.train(
    sentence_iterator=input_text(),
    model_prefix=model_name,
    max_sentence_length=65536,
    vocab_size=vanilla_tokenizer.vocab_size - len(vanilla_tokenizer.additional_special_tokens_ids),
    unk_id=none2neg1(vanilla_tokenizer.unk_token_id),
    bos_id=none2neg1(vanilla_tokenizer.bos_token_id),
    eos_id=none2neg1(vanilla_tokenizer.eos_token_id),
    pad_id=none2neg1(vanilla_tokenizer.pad_token_id),
    model_type="unigram",
    train_extremely_large_corpus=True,
)
