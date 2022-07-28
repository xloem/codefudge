#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <vector>

using namespace std;

#include <cppgit2/repository.hpp>

using namespace cppgit2;

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "tinytokenizers.rs.h"

struct oid_hash
{
    std::size_t operator() (const cppgit2::oid & oid) const
    {
        uint8_t const * start = (uint8_t const*)oid.c_ptr();
        uint8_t const * end = (uint8_t const*)oid.c_ptr() + sizeof(*oid.c_ptr());
        size_t output = 0;
        while (start != end) {
            if (end - start > sizeof(size_t)) {
                output ^= *(size_t*)(start);
                start += sizeof(size_t);
            } else if (end - start > sizeof(unsigned int)) {
                output ^= *(unsigned int *)(start);
                start += sizeof(unsigned int);
            } else {
                break;
            }
        }
        return output;
        // oid is 20 bytes long i think according to docs
        //uint32_t const * u32_data = reinterpret_cast<uint32_t const *>(oid.c_ptr());
        //uint64_t const * u64_data = reinterpret_cast<uint64_t const *>(oid.c_ptr());
        //return u64_data[0] ^ u64_data[1] ^ u32_data[4];
    }
};

struct oid_pair_hash
{
    template <class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2> & pair) const {
        return oid_hash()(pair.first) ^ oid_hash()(pair.second);
    }
};

int main(int argc, char **argv)
{
    int max_diffs_per_commit = 1;
    int max_commits_per_repo = 1;
    int seed = 0;
    unsigned int max_input_length = ~0;
    unsigned int max_output_length = 1024;
    bool cut_input = false;
    bool cut_output = false;
    string tokenizer_path = "tokenizer.json";
    string input_start = "";
    string message_start = "";
    string message_end = "";
    string file_name_start = "<pad>";
    string file_name_end = "<pad>";
    string file_content_start = "";
    string file_content_end = "";
    string input_end = "</s>";

    srand(seed);

    for (char **pathptr = &argv[1]; pathptr != &argv[argc]; ++ pathptr)
    {
        auto repository = repository::open(*pathptr);
        static thread_local vector<oid> commit_oids;
        repository.for_each_commit([](const commit & c)
        {
            commit_oids.push_back(c.id());
        });
        random_shuffle(commit_oids.begin(), commit_oids.end());

        int commits_output = 0;
        for (int commit_idx = 0; commits_output < max_commits_per_repo && commit_idx < commit_oids.size(); ++ commit_idx) {
            auto commit = repository.lookup_commit(commit_oids[commit_idx]);

            static thread_local cppgit2::index possible_conflicts, merges;
            cppgit2::diff diff;

            // commit.id().to_hex_string()
            // commit.message() commit.message_encoding()
            // commit.parent(n)  commit.parent_count()
            // commit.tree()


            switch (commit.parent_count()) {
            case 0:
                diff = repository.create_diff_tree_to_tree(tree(), commit.tree());
                break;
            case 1:
                diff = repository.create_diff_tree_to_tree(commit.parent(0).tree(), commit.tree());
                break;
            case 2:
                possible_conflicts = repository.merge_commits(commit.parent(0), commit.parent(1));
                merges.clear();
                merges.read_tree(commit.tree());
                diff = repository.create_diff_index_to_index(possible_conflicts, merges);
                break;
            default:
                throw "multimerge";
            }

            diff.find_similar(); // finds renames, copies. can have options passed.
            // diff.to_string()
            // diff.for_each([](const cppgit2::diff::delta &, float) {})
            // diff.print(diff:format, [](const diff

            // OPTIMIZE: this selection of inputs can be done by randomizing a range of integers and picking by integers
            // no need to enumerate them all in the next block

            static thread_local rust::Box<Tokenizer> tokenizer = from_file(tokenizer_path);
            static thread_local rust::Box<Encoding> tokenization = rust::Box<Encoding>::from_raw(nullptr);

                // use of pointer here will be referencing temporaries.
                // can likely use an oid pair ... or something
            static thread_local unordered_map<pair<oid,oid>, string, oid_pair_hash> inputs;
            //static thread_local unordered_map<pair<oid,oid>, vector<uint32_t>, oid_pair_hash> input_ids;
            static thread_local vector<pair<oid,oid>> input_index;
            diff.for_each([&](const cppgit2::diff::delta & need_eeg_and_blockchain, float progress) // pain shown
            { // input files
                auto old_id = need_eeg_and_blockchain.old_file().id();
                if (!old_id.is_zero()) {
                    blob content = repository.lookup_blob(need_eeg_and_blockchain.old_file().id());
                    if (!content.is_binary()) {
                        auto ident = make_pair<oid,oid>(
                            need_eeg_and_blockchain.old_file().id(),
                            need_eeg_and_blockchain.new_file().id()
                        ); // .path()
                        string & input = inputs[ident];
                        input += file_name_start + need_eeg_and_blockchain.old_file().path() + file_name_end + file_content_start;
                        input += string((char*)content.raw_contents(), content.raw_size());
                        input += file_content_end;
                        input_index.push_back(ident);
                    }
                }
            });

            static thread_local unordered_map<pair<oid,oid>, string, oid_pair_hash> outputs;
            static thread_local vector<pair<oid,oid>> diff_oids;
            diff.print(cppgit2::diff::format::patch, [&](
                const cppgit2::diff::delta & need_eeg_and_blockchain,
                const cppgit2::diff::hunk & hunk,
                const cppgit2::diff::line & line)
            {
                auto ident = make_pair<oid,oid>(
                    need_eeg_and_blockchain.old_file().id(),
                    need_eeg_and_blockchain.new_file().id()
                ); // .path()

                if (!diff_oids.size()) {
                    diff_oids.push_back(ident);
                } else if (diff_oids.back() != ident) {
                    diff_oids.push_back(ident);
                }
                string & patch = outputs[ident];
                char origin = line.origin();
                switch (origin) {
                case '-': case '+': case ' ':
                    patch += line.origin();
                default:;
                }
                patch.append(line.content(), line.content_length());
                /*tokenization = tokenizer->encode(patch, true);
                size_t patch_size = tokenization->get_ids().size();
                if (patch_token_ids.size() < max_output_length) {
                    static thread_local string line_content;
                    line_content = line.origin();
                    line_content += line.content();
                    tokenization = tokenizer->encode(line_content, true);
                    if (cut_output || patch.size() + line.content_length() + 1 <= max_output_length) {
                        patch += line.origin();
                        patch += line.content();
                        if (patch.size() > max_output_length) {
                            patch.resize(max_output_length);
                        }
                    }
                }*/
            });

            // we now have output data, indexed by diff_oids
            size_t total = diff.size() - diff.size(cppgit2::diff::delta::type::unmodified);
            random_shuffle(diff_oids.begin(), diff_oids.end());

            int diffs_output = 0;
            for(int diff_idx = 0; diffs_output < max_diffs_per_commit && diff_idx < total; ++ diff_idx) {
                auto ident = diff_oids[diff_idx];
                
                string output = outputs[ident];
                tokenization = tokenizer->encode(output, true);
                size_t output_size = tokenization->get_ids().size();
                if (output_size > max_output_length) {
                    if (!cut_output) {
                        continue;
                    } else {
                        // BUG: not cutting output due to tokenization funcs haven't implemented yet
                    }
                }

                string input = 
                    input_start + message_start + commit.message() + message_end;
                input += inputs[ident];
                tokenization = tokenizer->encode(input, true);
                size_t input_size = tokenization->get_ids().size();
                static thread_local vector<pair<oid,oid>> input_index_2;
                // first output context from other changed files
                input_index_2 = diff_oids;
                random_shuffle(input_index_2.begin(), input_index_2.end());
                while (input_size < max_input_length && !input_index_2.empty()) {
                    pair<oid,oid> & ident2 = input_index_2.back();
                    if (ident2 != ident) {
                        auto & more = inputs[ident2];
                        tokenization = tokenizer->encode(more, false);
                        size_t more_size = tokenization->get_ids().size();
                        if (cut_input || input_size + more_size <= max_input_length) {
                            input += inputs[ident2];
                            input_size += more_size;
                        }
                    }
                    input_index_2.pop_back();
                }
                if (input_size < max_input_length) {
                    // if room, add context from other files in the tree
                    input_index_2 = input_index;
                    random_shuffle(input_index_2.begin(), input_index_2.end());
                    while (input_size < max_input_length && !input_index_2.empty()) {
                        pair<oid,oid> & ident2 = input_index_2.back();
                        if (!outputs.count(ident2)) {
                            auto & more = inputs[ident2];
                            tokenization = tokenizer->encode(more, false);
                            size_t more_size = tokenization->get_ids().size();
                            if (cut_input || input_size + more_size <= max_input_length) {
                                input += inputs[ident2];
                                input_size += more_size;
                            }
                        }
                        input_index_2.pop_back();
                    }
                }
                // BUG: haven't implemented functions in tokenizer to find offset, so not cutting input
                //if (input_size > max_input_length) {
                //    input.resize(max_input_length);
                //}

                if (!input_size) {
                    continue;
                }

                static thread_local rapidjson::StringBuffer linebuf;
                static thread_local rapidjson::Writer<rapidjson::StringBuffer> lineout(linebuf);
                linebuf.Clear();
                //linedoc.Accept(lineout);
                lineout.StartObject();
                lineout.String("input", 5); lineout.String(input.data(), input.size());
                lineout.String("label", 5); lineout.String(output.data(), output.size());
                lineout.EndObject();
                linebuf.Put('\n');
                puts(linebuf.GetString());
                ++ diffs_output;
            }
            if (diffs_output > 0) {
                ++ commits_output;
            }
        }
    }
}
