use tokenizers::tokenizer::{Result, Tokenizer as HFTokenizer, Encoding as HFEncoding};

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        type Tokenizer;
        type Encoding;
        fn from_file(file: &String) -> Result<Box<Tokenizer>>;
        fn encode(self: &Tokenizer, input: String, add_special_tokens: bool) -> Result<Box<Encoding>>;
        fn get_ids(self: &Encoding) -> &[u32];
    }
}

fn from_file(file: &String) -> Result<Box<Tokenizer>> {
    Ok(Box::new(Tokenizer{tokenizer:HFTokenizer::from_file(file)?}))
}

struct Tokenizer {
    tokenizer: HFTokenizer,
}

impl Tokenizer {
    fn encode(&self, input: String, add_special_tokens: bool) -> Result<Box<Encoding>> {
        //Ok(self.tokenizer.encode(input, add_special_tokens)?.get_word_ids().into_iter().map(|&v| v.unwrap_or_default()).collect::<Vec<u32>>());
        Ok(Box::new(Encoding{encoding:self.tokenizer.encode(input, add_special_tokens)?}))
    }
}

struct Encoding {
    encoding: HFEncoding
}

impl Encoding {
    fn get_ids(&self) -> &[u32] {
        self.encoding.get_ids()
    }
}
