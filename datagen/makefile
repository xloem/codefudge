#TOKENIZERS_LDLIBS=-ltinytokenizers -pthread -lssl -lcrypto -ldl
#TOKENIZERS_LDFLAGS=-Ltinytokenizers/target/release
#TOKENIZERS_CPPFLAGS=-Itinytokenizers/target/cxxbridge/tinytokenizers/src -DTOKENIZE
#TOKENIZERS_DEPPATH=tinytokenizers/target/release/libtinytokenizers.a

LDLIBS=-lcppgit2 -lgit2 $(TOKENIZERS_LDLIBS)
CPPFLAGS=$(TOKENIZERS_CPPFLAGS)
LDFLAGS=$(TOKENIZERS_LDFLAGS)

CXXFLAGS+=-ggdb -O3

process2: process2.cpp $(TOKENIZERS_DEPPATH)

clean:
	-rm process2

tinytokenizers/target/release/libtinytokenizers.a: tinytokenizers/src/tinytokenizers.rs
	cd tinytokenizers && cargo build --release
