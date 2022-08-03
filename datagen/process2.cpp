#include <algorithm>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <unordered_set>
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
    std::unordered_map<std::string, std::unordered_set<cppgit2::oid, oid_hash>> missing_objects_by_remote_name;

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

    void missing(cppgit2::oid const & missing_object, cppgit2::oid const & commit)
    {
        bool found = false;
        repository.for_each_branch([&](cppgit2::reference branch)
        {
            auto branch_tip = branch.resolve().target();
            if (repository.is_descendant_of(branch_tip, commit)) {
                // the remote containing branch should have the missing object
                std::string remote_name = repository.branch_remote_name(branch.name());
                cerr << missing_object.to_hex_string() << " is missing; " << branch.name() << " should contain it from " << commit.to_hex_string() << endl;
                missing_objects_by_remote_name[remote_name].insert(missing_object);
                found = true;
            }
        }, cppgit2::branch::branch_type::remote);
        if (!found) {
            throw std::string("no remote had " + missing_object.to_hex_string());
        }
    }

    void missing(std::string const & missing_object, cppgit2::oid const & commit)
    {
        missing(cppgit2::oid(missing_object), commit);
    }

    void missing(cppgit2::git_exception const &exc, cppgit2::oid const & commit)
    {
        string msg = exc.what();
        size_t end = msg.rfind(')');
        size_t start = msg.rfind('(', end);
        if (end == std::string::npos || start == std::string::npos) {
            throw exc;
        }
        start += 1;
        missing(msg.substr(start, end - start), commit);
    }

    bool fetch_missing()
    {
        if (missing_objects_by_remote_name.empty()) {
            return false;
        }

        for (auto & remote_name_and_missing_objects : missing_objects_by_remote_name) {
            cerr << "Downloading";
            auto & remote_name = remote_name_and_missing_objects.first;
            auto & missing_objects = remote_name_and_missing_objects.second;
            string cmd = "cd '" + repository.path() + "';{ ";
            for (auto & missing_object : missing_objects) {
                string id = missing_object.to_hex_string();
                cerr << " " << id;
                cmd += "echo " + id + "; ";
            }
            cerr << " ..." << endl;
            cmd += "} | git -c remote.'" + remote_name + "'.promisor=true cat-file --batch-check 1>&2";
            cerr << cmd << endl;
            //string cmd = "cd '" + repository.path() + "';git cat-file blob " + oid + ">/dev/null";
            //cerr << cmd << endl;
            if (system(cmd.c_str())) {
                throw std::string("batch git cat-file subprocess failed\n" + cmd);
            }
            missing_objects.clear();
        }
        missing_objects_by_remote_name.clear();
        return true;
                        /*repository.for_each_branch([&](cppgit2::reference branch)
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
                        */
    }
};

struct output_manager
{
    output_manager(
        size_t seed,
        size_t max_input_length,
        size_t max_output_length,
        #ifdef TOKENIZE
        bool lengths_are_tokenized,
        #endif
        bool cut_input,
        bool cut_output,
        string tokenizer_path,
        string input_start,
        string message_start,
        string message_end,
        string file_name_start,
        string file_name_end,
        string file_content_start,
        string file_content_end,
        string input_end
    )
    : rng{seed},
      max_input_length(max_input_length),
      max_output_length(max_output_length),
      #ifdef TOKENIZE
      lengths_are_tokenized(lengths_are_tokenized),
      #endif
      cut_input(cut_input),
      cut_output(cut_output),
      tokenizer_path(tokenizer_path),
      input_start(input_start),
      message_start(message_start),
      message_end(message_end),
      file_name_start(file_name_start),
      file_name_end(file_name_end),
      file_content_start(file_content_start),
      file_content_end(file_content_end),
      input_end(input_end)
    { }

    void init_commit(repo_commits * repo_entry, cppgit2::commit * commit, cppgit2::diff * diff)
    {
        cerr << "commit: " << commit->id().to_hex_string() << endl;
        this->commit = commit;
        this->repo_entry = repo_entry;
        this->diff = diff;
        diff_idcs.resize(diff->size());
        for (size_t i = 0; i < diff->size(); ++i) {
            diff_idcs[i] = i;
        }
        diff_subidcs = diff_idcs;
        std::shuffle(diff_idcs.begin(), diff_idcs.end(), rng);

        output_diff_idcs.clear();

        // it is probably not hard to look into this call and do it for only one file when needed. it just references substrings with prefixes. diff as a whole can also be called on individual files.
        cerr << "collecting output: " << commit->id().to_hex_string() << endl;
        outputs.clear();
        diff->print(cppgit2::diff::format::patch, [&](
            const cppgit2::diff::delta & need_eeg_and_blockchain,
            const cppgit2::diff::hunk & hunk,
            const cppgit2::diff::line & line)
        {
            //cerr << "Making output: " << need_eeg_and_blockchain.new_file().path() << endl;
            string & patch = outputs[need_eeg_and_blockchain.new_file().id()];
            char origin = line.origin();
            switch (origin) {
            case '-': case '+': case ' ':
                patch += line.origin();
            default:;
            }
            patch.append(line.content(), line.content_length());
        });
    }

    size_t process(size_t max_diffs_per_commit)
    {
        size_t output_size = 0;
        size_t diffs_output = 0;
        output_diff_idcs.clear();
        cerr << "looping over diff: " << commit->id().to_hex_string() << endl;
try_more:
        for (size_t diff_idx = 0; diffs_output < max_diffs_per_commit && diff_idx < diff_idcs.size(); ++ diff_idx)
        {
            size_t idx = diff_idcs[diff_idx];
            const cppgit2::diff::delta & need_eeg_and_blockchain = (*diff)[idx];

            decltype(outputs)::iterator output_it = outputs.find(need_eeg_and_blockchain.new_file().id());
            if (output_it == outputs.end()) {
                continue;
            }

            std::string & output = output_it->second;

            #ifdef TOKENIZE
            if (lengths_are_tokenized) {
                tokenization = tokenizer->encode(output, true);
                output_size = tokenization->get_ids().size();
            } else
            #endif
            {
                output_size = output.size();
            }
            // BUGS: cut_input and cut_output not honored at all
            if (!cut_output && output_size > max_output_length) {
                continue;
            }

            output_diff_idcs.insert(idx);

            input = input_start + message_start + commit->message() + message_end;
            
            if (!add_input(need_eeg_and_blockchain)) {
                continue;
            }

            std::shuffle(diff_subidcs.begin(), diff_subidcs.end(), rng);
            for (size_t diff_subidx = 0; input_size < max_input_length && diff_subidx < diff_subidcs.size(); ++ diff_subidx)
            {
                size_t idx = diff_subidcs[diff_subidx];
                if (output_diff_idcs.count(idx)) {
                    continue;
                }
                const cppgit2::diff::delta & need_eeg_and_blockchain = (*diff)[idx];
                add_input(need_eeg_and_blockchain);
            }

            output_diff_idcs.erase(idx);

            static thread_local rapidjson::StringBuffer linebuf;
            static thread_local rapidjson::Writer<rapidjson::StringBuffer> lineout;
            linebuf.Clear();
            lineout.Reset(linebuf);
            lineout.StartObject();
            lineout.String("input", 5); lineout.String(input.data(), input.size());
            lineout.String("label", 5); lineout.String(output.data(), output.size());
            lineout.EndObject();
            puts(linebuf.GetString());
            ++ diffs_output;
        }
        if (repo_entry->fetch_missing()) {
            goto try_more;
        }
        return diffs_output;
    }

    bool add_input(const cppgit2::diff::delta & need_eeg_and_blockchain)
    {
        auto old_file = need_eeg_and_blockchain.old_file();
        auto old_id = old_file.id();
        more_input_size = 0;

        if (
            (old_file.mode() & 0777000) != (GIT_FILEMODE_BLOB & 0777000) ||
            old_id.is_zero() ||
            old_file.flags() & (uint32_t)cppgit2::diff::delta::flag::binary
            #ifndef TOKENIZE
            || (!cut_input && old_file.size() > max_input_length)
            #endif
        ) {
            return false;
        }

        blob content;
        try {
            content = repo_entry->repository.lookup_blob(old_id);
        } catch (cppgit2::git_exception &exc) {
            repo_entry->missing(exc, commit->id());
            return false;
        }

        more_input
            = file_name_start
            + old_file.path()
            + file_name_end
            + file_content_start
            + string((char*)content.raw_contents(), content.raw_size())
            + file_content_end
        ;
        #ifdef TOKENIZE
        if (lengths_are_tokenized) {
            tokenization = tokenizer->encode(input, true);
            more_input_size = tokenization->get_ids().size();
        } else
        #endif
        {
            more_input_size = input.size();
        }
        if (!cut_input && input_size + more_input_size > max_input_length) {
            return false;
        }
        input += more_input;
        input_size += more_input_size;
        return true;
    }

    std::default_random_engine rng;
    size_t seed;
    size_t max_input_length;
    size_t max_output_length;
    #ifdef TOKENIZE
    bool lengths_are_tokenized;
    #endif
    bool cut_input;
    bool cut_output;
    std::string tokenizer_path;
    std::string input_start;
    std::string message_start;
    std::string message_end;
    std::string file_name_start;
    std::string file_name_end;
    std::string file_content_start;
    std::string file_content_end;
    std::string input_end;
    repo_commits * repo_entry;
    cppgit2::commit const * commit;
    cppgit2::diff const * diff;
    std::vector<size_t> diff_idcs, diff_subidcs;
    std::unordered_set<size_t> output_diff_idcs;
    std::unordered_map<cppgit2::oid, std::string, oid_hash> outputs;
    cppgit2::blob content;
    std::string more_input;
    std::string input, output;
    size_t more_input_size, input_size;
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
    static thread_local output_manager outputter(
        seed,
        max_input_length,
        max_output_length,
        #ifdef TOKENIZE
        lengths_are_tokenized,
        #endif
        cut_input,
        cut_output,
        tokenizer_path,
        input_start,
        message_start,
        message_end,
        file_name_start,
        file_name_end,
        file_content_start,
        file_content_end,
        input_end
    );

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

            // this could simply select a random index repeatedly
            shuffle(commit_oids.begin(), commit_oids.end(), rng);
    
            int commits_output = 0;
            for (int commit_idx = 0; commits_output < max_commits_per_repo && commit_idx < commit_oids.size(); ++ commit_idx) {
                cppgit2::commit commit;
                try {
                    commit = repository.lookup_commit(commit_oids[commit_idx]);
                } catch (cppgit2::git_exception &exc) {
                    repo_entry.missing(exc, commit_oids[commit_idx]);
                    repo_entry.fetch_missing();
                    commit = repository.lookup_commit(commit_oids[commit_idx]);
                }
    
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
    
                outputter.init_commit(&repo_entry, &commit, &diff);
                if (outputter.process(max_diffs_per_commit)) {
                    commits_output ++;
                }
#if 0
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
                                // process objects that are blobs with nonzero ids. object type is stored in file mode.
                                if ((file.mode() & 0777000) == (GIT_FILEMODE_BLOB & 0777000) && !id.is_zero()) {
                                    try {
                                        repository.lookup_blob(id);
                                    } catch (cppgit2::git_exception &exc) {
                                        repo_entry.missing(id, commit.id());
                                    }
                                }
                            }
                        });
                    } catch (cppgit2::git_exception &exc) {
                        repo_entry.missing(exc, commit.id());
                    }
                    if (repo_entry.fetch_missing()) {
                        continue;
                    } else{
                        break;
                    }
                }
                diff.for_each([&](const cppgit2::diff::delta & need_eeg_and_blockchain, float progress) // pain shown
                { // input files
                    auto old_file = need_eeg_and_blockchain.old_file();
                    auto old_id = old_file.id();
                    if ((old_file.mode() & 0777000) == (GIT_FILEMODE_BLOB & 0777000) && !old_id.is_zero()) {
                                    //cerr << "looking up content: " << need_eeg_and_blockchain.old_file().id().to_hex_string() << endl;
                        blob content = repository.lookup_blob(old_id);
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

                // new organisation
                static thread_local vector<size_t> diff_idcs, diff_subidcs;
                diff_idcs.resize(diff.size());
                for (size_t i = 0; i < diff.size; ++i ) {
                        diff_idcs[i] = i;
                }
                diff_subidcs = diff_idcs;
iterate:
                // shuffling rather than picking randomly repeatedly lets us download all the missing objects after a loop
                shuffle(diff_idcs.begin(), diff_idcs.end(), rng);
                static thread_local std::unordered_set<int> output_diff_idcs;
                for (int diff_idx = 0; diffs_output < max_diffs_per_commit && diff_idx < diff_idcs.size(); ++ diff_idx) {
                    const cppgit2::diff::delta & need_eeg_and_blockchain = diff[diff_idcs[diff_idx]];
                    auto old_file = need_eeg_and_blockchain.old_file();
                    auto old_id = old_file.id();
                    size_t input_size = 0;

                    if (
                        (old_file.mode() & 0777000) != (GIT_FILEMODE_BLOB & 0777000) ||
                        old_id.is_zero() ||
                        old_file.flags() && cppgit2::diff::flag::binary
#ifndef TOKENIZE
                        || (!cut_input && old_file.size() > max_input_length)
#endif
                    ) {
                        continue;
                    }

                    static thread_local blob content;
                    try {
                        content = repository.lookup_blob(old_id);
                    } catch (cppgit2::git_exception &exc) {
                        repo_entry.missing(exc, commit.id());
                        continue;
                    }

                    static thread_local string input;
                    input.clear();
                    input_size = 0;
                    output_diff_idcs.reset();
                    input
                        += file_name_start
                        + need_eeg_and_blockchain.old_file.path()
                        + file_name_end
                        + file_content_start
                        + string((char*)content.raw_contents(), content.raw_size())
                        + file_content_end
                    ;
                        if (lengths_are_tokenized) {
                            tokenization = tokenizer->encode(input, true);
                            input_size = tokenization->get_ids().size();
                        } else
                        {
                            input_size = input.size();
                        }
                    input_size += input.size();

                    output_diff_idcs.insert(diff_idx);

                    shuffle(diff_subidcs.begin(), diff_subidcs.end(), rng);
                    for (int diff_subidx = 0; diff_subidx < diff_subidcs.size(); ++ diff_subidx) {
                        if (output_diff_idcs.count(diff_subidcs[diff_subidx]) {
                            continue;
                        }
                        const cppgit2::diff::delta & need_eeg_and_blockchain = diff[diff_idcs[diff_idx]];
                        auto old_file = need_eeg_and_blockchain.old_file();
                        auto old_id = old_file.id();
                        if (
                            (old_file.mode() & 0777000) != (GIT_FILEMODE_BLOB & 0777000) ||
                            old_id.is_zero() ||
                            old_file.flags() && cppgit2::diff::flag::binary
#ifndef TOKENIZE
                            || (!cut_input && input_size + old_file.size() > max_input_length)
#endif
                        ) {
                            continue;
                        }
                        try {
                            content = repository.lookup_blob(old_id);
                        } catch (cppgit2::git_exception &exc) {
                            repo_entry.missing(exc, commit.id());
                            continue;
                        }
                        input
                            += file_name_start
                            + need_eeg_and_blockchain.old_file.path()
                            + file_name_end
                            + file_content_start
                            + string((char*)content.raw_contents(), content.raw_size())
                            + file_content_end
                        ;
                        input_size += input.size();
                        output_diff_idcs.insert(diff_idx);
                        // INPUT left off here
                        //   // the above code happens twice 
                    }


                    
                
                    // OUTPUT
                    string output = outputs[ident];

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
                    output_size = output.size();
                //    }
                }
                if (repo_entry.fetch_missing()) {
                    goto iterate;
                }
    
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
                    puts(linebuf.GetString());
                    ++ diffs_output;
                }
                if (diffs_output > 0) {
                    ++ commits_output;
                }
#endif
            }
        }
    }
}
