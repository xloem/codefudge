#include <algorithm>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>

using namespace std;

#include <cppgit2/repository.hpp>

using namespace cppgit2;

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#ifdef TOKENIZE
#include "tinytokenizers.rs.h"
#endif

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
        return oid_hash()(pair.first) ^ (oid_hash()(pair.second) << 1);
    }
};

struct repo_commits
{
    cppgit2::repository repository;
    std::vector<cppgit2::oid> commits;
    repo_commits(char const * path)
    : repository(cppgit2::repository::open(path))
    {
        std::cerr << "Loading commits for " << path << std::endl;
        //static cppgit2::revwalk revwalk;
	//revwalk.reset();
        auto revwalk = repository.create_revwalk();
        revwalk.push_glob("*");
        while (!revwalk.done()) {
            commits.push_back(revwalk.next());
        }
    }
};

int main(int argc, char **argv)
{
    unsigned int max_diffs_per_commit = 1;
    unsigned int max_commits_per_repo = 1;
    unsigned int seed = 0;
    unsigned int max_input_length = 256; //~0;
    unsigned int max_output_length = 256; //~0; //1024;
    unsigned int cycles_over_repos = 16; //2;//~0;
    #ifdef TOKENIZE
    bool lengths_are_tokenized = false;
    #endif
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

    default_random_engine rng{seed};

    static thread_local unordered_map<string, repo_commits> repos;

    diff::options diff_options;
    diff_options.set_flags(
        diff::options::flag::include_unmodified |
        diff::options::flag::include_typechange |
        diff::options::flag::ignore_filemode |
        diff::options::flag::ignore_submodules |
        diff::options::flag::indent_heuristic |
        diff::options::flag::patience |
        diff::options::flag::minimal |
        diff::options::flag::show_binary
    );

    for (unsigned int repo_cycle = 0; repo_cycle < cycles_over_repos; ++ repo_cycle)
    {
        for (char **pathptr = &argv[1]; pathptr != &argv[argc]; ++ pathptr)
        {
            if (!repos.count(*pathptr)) {
                repos.emplace(*pathptr, *pathptr);
            }
            repo_commits & repo_entry = repos.at(*pathptr);
            auto & repository = repo_entry.repository;
            auto & commit_oids = repo_entry.commits;

            shuffle(commit_oids.begin(), commit_oids.end(), rng);
    
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
                    diff = repository.create_diff_tree_to_tree(tree(), commit.tree(), diff_options);
                    break;
                case 1:
                    diff = repository.create_diff_tree_to_tree(commit.parent(0).tree(), commit.tree(), diff_options);
                    break;
                case 2:
                    possible_conflicts = repository.merge_commits(commit.parent(0), commit.parent(1));
                    merges.clear();
                    merges.read_tree(commit.tree());
                    diff = repository.create_diff_index_to_index(possible_conflicts, merges, diff_options);
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
    
		#ifdef TOKENIZE
                static thread_local rust::Box<Tokenizer> tokenizer = from_file(tokenizer_path);
                static thread_local rust::Box<Encoding> tokenization = rust::Box<Encoding>::from_raw(nullptr);
		#endif
    
                    // use of pointer here will be referencing temporaries.
                    // can likely use an oid pair ... or something
                static thread_local unordered_map<pair<oid,oid>, string, oid_pair_hash> inputs;
                //static thread_local unordered_map<pair<oid,oid>, vector<uint32_t>, oid_pair_hash> input_ids;
                static thread_local vector<pair<oid,oid>> input_index;
                input_index.clear();
                inputs.clear();

                cerr << "looping over diff: " << commit.id().to_hex_string() << endl;
                // fetch missing objects?
                while ("checking all objects are present") {
                    try {
                        diff.for_each([&](const cppgit2::diff::delta & need_eeg_and_blockchain, float progress) {
                            cppgit2::diff::delta::file files[2] = {need_eeg_and_blockchain.old_file(), need_eeg_and_blockchain.new_file()};
                            for (auto & file : files) {
                                cppgit2::oid id = file.id();
                                if (!id.is_zero()) {
                                    try{
                                        repository.lookup_object(id, object::object_type::any);
                                    } catch (cppgit2::git_exception &exc) {
                                        static string msg;
                                        msg = exc.what();
                                        try {
                                            // submodule ids are the submodule head commit and are never present
                                            repository.lookup_submodule(need_eeg_and_blockchain.old_file().path());
                                            continue;
                                        } catch (cppgit2::git_exception &exc2) {
                                            cerr << exc2.what() << endl;
                                            throw cppgit2::git_exception(msg); // the original exception had its message overwritten by the following exception
                                        }
                                    }
                                }
                            }
                        });
                        break;
                    } catch (cppgit2::git_exception &exc) {
                        string msg = exc.what();
                        size_t end = msg.rfind(')');
                        size_t start = msg.rfind('(', end) + 1;
                        string oid = msg.substr(start, end - start);
                        cppgit2::oid oidoid(oid);

                        cerr << "Downloading " << oid << " ..." << endl;

                        repository.for_each_branch([&](cppgit2::reference branch)
                        {
                            if (repository.is_descendant_of(branch.resolve().target(), commit.id())) {
                                    // the remote containing branch should have oid
                                    std::string remote_name = repository.branch_remote_name(branch.name());
                                    cppgit2::remote remote = repository.lookup_remote(remote_name);
                                    //if (!remote.is_connected()) {
                                    //    remote.connect(cppgit2::connection_direction::fetch);
                                    //}
                                    std::vector<std::string> refspecs;
                                    std::string missing_commit = commit.id().to_hex_string();
                                    refspecs.push_back(missing_commit + ":" + remote_name + "/single_commit");
                                    //refspecs.push_back(branch.name() + ":" + remote_name + "/" + branch.name());
                                    cppgit2::strarray refspecs_array(refspecs);
                                    remote.download(refspecs_array);
                                    //string cmd = "cd '" + repository.path() + "'; git fetch " + 
                            }
                        }, cppgit2::branch::branch_type::remote);

                        // this can be done directly by using something like repository.mergebase to check
                        // each remote branch for the commit using remote.for_each_branch, then fetching from the remote
                        //string cmd = "cd '" + repository.path() + "';git cat-file blob " + oid + ">/dev/null";
                        //cerr << cmd << endl;
                        //if (system(cmd.c_str())) {
                        //    throw;
                       // }
                    }
                }
                diff.for_each([&](const cppgit2::diff::delta & need_eeg_and_blockchain, float progress) // pain shown
                { // input files
                    auto old_id = need_eeg_and_blockchain.old_file().id();
                    if (!old_id.is_zero()) {
		    	        //cerr << "looking up content: " << need_eeg_and_blockchain.old_file().id().to_hex_string() << endl;
                        blob content = repository.lookup_blob(need_eeg_and_blockchain.old_file().id());
                        if (!content.is_binary()) {
                            auto ident = make_pair<oid,oid>(
                                need_eeg_and_blockchain.old_file().id(),
                                need_eeg_and_blockchain.new_file().id()
                            ); // .path()
                            if (!inputs.count(ident)) {
                                string & input = inputs[ident];
                                input += file_name_start + need_eeg_and_blockchain.old_file().path() + file_name_end + file_content_start;
                                input += string((char*)content.raw_contents(), content.raw_size());
                                input += file_content_end;
                            }
                            input_index.push_back(ident);
                        }
                    }
                });

		//cout <<  "input count: " << inputs.size() << endl;
    
                static thread_local unordered_map<pair<oid,oid>, string, oid_pair_hash> outputs;
                static thread_local vector<pair<oid,oid>> diff_oids;
                diff_oids.clear();
                outputs.clear();
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
                });

		//cout <<  "output count: " << outputs.size() << endl;
    
                // we now have output data, indexed by diff_oids
                size_t total = diff.size() - diff.size(cppgit2::diff::delta::type::unmodified);
                shuffle(diff_oids.begin(), diff_oids.end(), rng);
                int diffs_output = 0;

                //static thread_local vector<size_t> diff_idcs;
                //diff_idcs.resize(diff.size());
                //for (size_t i = 0; i < diff.size; ++i ) {
                //	diff_idcs[i] = i;
                //}
                //shuffle(diff_idcs.begin(), diff_idcs.end(), rng);
                //for (int diff_idx = 0; diffs_output < max_diffs_per_commit && diff_idx < diff_idcs.size(); ++ diff_idx) {
                //    const cppgit2::diff::delta & need_eeg_and_blockchain = diff[diff_idcs[diff_idx]];
                //    auto ident = make_pair<oid,oid>(
                //        need_eeg_and_blockchain.old_file().id(),
                //        need_eeg_and_blockchain.new_file().id()
                //    ); // .path()
                //    
                //    // INPUT left off here
                //
                //    // OUTPUT
                //    string output = outputs[ident];

                //    #ifdef TOKENIZE
                //    if (lengths_are_tokenized) {
                //        tokenization = tokenizer->encode(output, true);
                //        output_size = tokenization->get_ids().size();
                //        if (output_size > max_output_length) {
                //            if (!cut_output) {
                //                continue;
                //            } else {
                //                // BUG: not cutting output due to tokenization funcs haven't implemented yet
                //            }
                //        }
                //    } else
                //    #endif
                //    {
                //        output_size = output.size();
                //    }
                //}
    
                for(int diff_idx = 0; diffs_output < max_diffs_per_commit && diff_idx < total; ++ diff_idx) {
                    auto ident = diff_oids[diff_idx];
                    //cout <<  "selecting " << ident.first.to_hex_string() << endl;

                    size_t output_size = 0, input_size = 0;
                    
                    string output = outputs[ident];
                    #ifdef TOKENIZE
                    if (lengths_are_tokenized) {
                        tokenization = tokenizer->encode(output, true);
                        output_size = tokenization->get_ids().size();
                        if (output_size > max_output_length) {
                            if (!cut_output) {
                                continue;
                            } else {
                                // BUG: not cutting output due to tokenization funcs haven't implemented yet
                            }
                        }
                    } else
		    #endif
                    {
                        output_size = output.size();
                    }
    
                    string input = 
                        input_start + message_start + commit.message() + message_end;
                    input += inputs[ident];
                    
		    #ifdef TOKENIZE
                    if (lengths_are_tokenized) {
                        tokenization = tokenizer->encode(input, true);
                        input_size = tokenization->get_ids().size();
                    } else
		    #endif
                    {
                        input_size = input.size();
                    }

                    static thread_local vector<pair<oid,oid>> input_index_2;
                    // first output context from other changed files
                    input_index_2 = diff_oids;
                    shuffle(input_index_2.begin(), input_index_2.end(), rng);
		    //cout <<  "input_size=" << input_size << endl;
		    //cout <<  "max_input_length=" << max_input_length << endl;
		    //cout <<  "input_index_2.size()=" << input_index_2.size() << endl;
                    while (input_size < max_input_length && !input_index_2.empty()) {
                        pair<oid,oid> & ident2 = input_index_2.back();
		        //cout <<  "considering " << ident.first.to_hex_string() << endl;
                        if (ident2 != ident) {
                            auto & more = inputs[ident2];
                            size_t more_size;
		            #ifdef TOKENIZE
                            if (lengths_are_tokenized) {
                                tokenization = tokenizer->encode(more, false);
                                more_size = tokenization->get_ids().size();
                            } else
                            #endif
                            {
                                more_size = more.size();
                            }
                            if (cut_input || input_size + more_size <= max_input_length) {
                                input += inputs[ident2];
                                input_size += more_size;
                            }
                        }
                        input_index_2.pop_back();
		        //cout <<  "input_size=" << input_size << endl;
		        //cout <<  "input_index_2.size()=" << input_index_2.size() << endl;
                    }
		    //cout <<  "input_size=" << input_size << endl;
		    //cout <<  "max_input_length=" << max_input_length << endl;
                    if (input_size < max_input_length) {
                        // if room, add context from other files in the tree
                        input_index_2 = input_index;
                        shuffle(input_index_2.begin(), input_index_2.end(), rng);
		        //cout <<  "input_size=" << input_size << endl;
		        //cout <<  "max_input_length=" << max_input_length << endl;
		        //cout <<  "input_index_2.size()=" << input_index_2.size() << endl;
                        while (input_size < max_input_length && !input_index_2.empty()) {
                            pair<oid,oid> & ident2 = input_index_2.back();
                            if (!outputs.count(ident2)) {
                                auto & more = inputs[ident2];
                                size_t more_size;
		                #ifdef TOKENIZE
                                if (lengths_are_tokenized) {
                                    tokenization = tokenizer->encode(more, false);
                                    more_size = tokenization->get_ids().size();
                                } else
		                #endif
                                {
                                    more_size = more.size();
                                }
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
                    static thread_local rapidjson::Writer<rapidjson::StringBuffer> lineout;
                    linebuf.Clear();
                    lineout.Reset(linebuf);
                    lineout.StartObject();
                    lineout.String("input", 5); lineout.String(input.data(), input.size());
                    lineout.String("label", 5); lineout.String(output.data(), output.size());
                    lineout.EndObject();
                    //puts(linebuf.GetString());
                    ++ diffs_output;
                }
                if (diffs_output > 0) {
                    ++ commits_output;
                }
            }
        }
    }
}
