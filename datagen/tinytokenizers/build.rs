fn main() {
    cxx_build::bridge("src/tinytokenizers.rs")
        .compile("tinytokenizers");
}
