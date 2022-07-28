TOKENIZERS_LDFLAGS=-ltinytokenizers -Ltinytokenizers/target/release -pthread -lssl -lcrypto -ldl
TOKENIZERS_CPPFLAGS=-Itinytokenizers/target/cxxbridge/tinytokenizers/src

LDFLAGS=-lcppgit2 -lgit2 $(TOKENIZERS_LDFLAGS)
CPPFLAGS=$(TOKENIZERS_CPPFLAGS)

CXXFLAGS+=-ggdb -O0

process2: process2.cpp tinytokenizers/target/release/libtinytokenizers.a

tinytokenizers/target/release/libtinytokenizers.a: tinytokenizers/src/tinytokenizers.rs
	cd tinytokenizers && cargo build --release
