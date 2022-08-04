#include <algorithm>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

#include <cppgit2/patch.hpp>
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
    std::string path;
    cppgit2::repository repository;
    std::vector<cppgit2::oid> commits;
    std::unordered_map<std::string, std::unordered_set<cppgit2::oid, oid_hash>> missing_objects_by_remote_name;


    repo_commits(char const * path)
    : path(path),
      repository(cppgit2::repository::open(path))
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

    std::string remote_branch_name(cppgit2::oid const & commit)
    {
        for (
            remote_branch_iter.init(repository, cppgit2::branch::branch_type::remote);
            remote_branch_iter;
            ++ remote_branch_iter
        ) {
            auto branch = *remote_branch_iter;
            auto branch_tip = branch.resolve().target();
            if (repository.is_descendant_of(branch_tip, commit)) {
                return branch.name();
            }
        }
        return {};
    }

    void missing(cppgit2::oid const & missing_object, cppgit2::oid const & commit)
    {
        // the remote containing branch should have the missing object
        std::string remote_branch_name = this->remote_branch_name(commit);
        if (remote_branch_name.empty()) {
            throw std::invalid_argument("no remote had " + missing_object.to_hex_string());
        }
        std::string remote_name = repository.branch_remote_name(remote_branch_name);
        cerr << missing_object.to_hex_string() << " is missing; " << remote_branch_name << " should contain it from " << commit.to_hex_string() << endl;
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
            string cmd = "{ ";
            for (auto & missing_object : missing_objects) {
                string id = missing_object.to_hex_string();
                cerr << " " << id;
                cmd += "echo " + id + "; ";
            }
            cerr << " ..." << endl;
            cmd += "} | git -C '" + repository.path() + "' -c remote.'" + remote_name + "'.promisor=true cat-file --batch-check 1>&2";
            cerr << cmd << endl;
            //string cmd = "cd '" + repository.path() + "';git cat-file blob " + oid + ">/dev/null";
            //cerr << cmd << endl;
            if (system(cmd.c_str())) {
                throw std::runtime_error("batch git cat-file subprocess failed\n" + cmd);
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

    struct branch_iterator {
        branch_iterator()
        : c_ptr(0), c_ref(0)
        { }
        void init(cppgit2::repository & repo, cppgit2::branch::branch_type type)
        {
            c_type = static_cast<git_branch_t>(type);
            git_branch_iterator_new(&c_ptr, const_cast<git_repository*>(repo.c_ptr()), c_type);
            ++(*this);
        }
        cppgit2::reference operator*()
        {
            return {c_ref};
        }
        void operator++()
        {
            if (git_branch_next(&c_ref, &c_type, c_ptr) != 0) {
                c_ref = 0;
            }
        }
        operator bool() {
            return c_ref;
        }
        void free()
        {
            if (c_ptr) {
                git_branch_iterator_free(c_ptr);
                c_ptr = 0;
                c_ref = 0;
            }
        }
        ~branch_iterator()
        {
            free();
        }
        git_branch_iterator *c_ptr;
        git_reference *c_ref;
        git_branch_t c_type;
    } remote_branch_iter;
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
        #ifdef TOKENIZE
        string tokenizer_path,
        #endif
        string input_start,
        string message_start,
        string message_end,
        string file_name_start,
        string file_name_end,
        string file_content_start,
        string file_content_end,
        string input_end,
        cppgit2::diff::options::flag diff_flags
    )
    : rng{seed},
      #ifdef TOKENIZE
      tokenizer(from_file(tokenizer_path)),
      #endif
      tokenizer_path(tokenizer_path),
      input_start(input_start),
      message_start(message_start),
      message_end(message_end),
      file_name_start(file_name_start),
      file_name_end(file_name_end),
      file_content_start(file_content_start),
      file_content_end(file_content_end),
      input_end(input_end)
    {
        input.max = max_input_length;
        output.max = max_output_length;
        #ifdef TOKENIZE
        if (lengths_are_tokenized) {
            input.max_token_ids = max_input_length;
            input.max = max_input_length * 4;
            input.tokenizer = &tokenizer;
            output.max_token_ids = max_output_length;
            output.max = max_output_length * 4;
            output.tokenizer = &tokenizer;
        } else {
            input.max_token_ids = 0;
            output.max_token_ids = 0;
        }
        #endif
        input.cut = cut_input;
        output.cut = cut_output;
        diff_options.set_flags(diff_flags);
    }

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

        skip_input_diff_idcs.clear();
    }

    size_t process(size_t max_diffs_per_commit)
    {
        size_t output_size = 0;
        size_t diffs_output = 0;
        skip_input_diff_idcs.clear();
        std::string commit_msg = commit->message();
        cerr << "looping over diff: " << commit->id().to_hex_string() << endl;
        if (!input.set(input_start.data(), input_start.size())) {
            throw std::invalid_argument("input_start does not fit in input");
        }
        if (!input.append(message_start.data(), message_start.size())) {
            throw std::invalid_argument("message_start does not fit in input");
        }
        auto commit_message = commit->message();
        if (!input.append(commit_message.data(), commit_message.size())) {
            return 0;
        }
        if (!input.append(message_end.data(), message_end.size())) {
            return 0;
        }
        more_input.to_append_to(input);
try_more:
        for (size_t diff_idx = 0; diffs_output < max_diffs_per_commit && diff_idx < diff_idcs.size(); ++ diff_idx)
        {
            size_t idx = diff_idcs[diff_idx];
            const cppgit2::diff::delta & need_eeg_and_blockchain = (*diff)[idx];
            
            more_input.clear();
            if (!add_input(need_eeg_and_blockchain, true)) {
                skip_input_diff_idcs.insert(idx);
                continue;
            }

            if (!get_output(idx)) {
                continue;
            }

            std::shuffle(diff_subidcs.begin(), diff_subidcs.end(), rng);
            for (size_t diff_subidx = 0; input.can_append(file_name_start.size() + file_name_end.size() + file_content_start.size() + file_content_end.size() + 16) && diff_subidx < diff_subidcs.size(); ++ diff_subidx)
            {
                size_t subidx = diff_subidcs[diff_subidx];
                if (idx == subidx || skip_input_diff_idcs.count(subidx)) {
                    continue;
                }
                const cppgit2::diff::delta & need_eeg_and_blockchain = (*diff)[idx];
                add_input(need_eeg_and_blockchain, false);
            }

            static thread_local rapidjson::StringBuffer linebuf;
            static thread_local rapidjson::Writer<rapidjson::StringBuffer> lineout;
            linebuf.Clear();
            lineout.Reset(linebuf);
            lineout.StartObject();
            more_input.data = input.data + more_input.data;
            lineout.String("input", 5); lineout.String(more_input.data.data(), more_input.data.size());
            lineout.String("label", 5); lineout.String(output.data.data(), output.data.size());
            auto commit_id = commit->id();
            auto remote_branch_name = repo_entry->remote_branch_name(commit_id);
            if (!remote_branch_name.empty()) {
                lineout.String("branch", 6); lineout.String(remote_branch_name.data(), remote_branch_name.size());
            }
            auto commit_string = commit_id.to_hex_string();
            lineout.String("commit", 6); lineout.String(commit_string.data(), commit_string.size());
            lineout.String("repo", 4); lineout.String(repo_entry->path.data(), repo_entry->path.size());
            lineout.EndObject();
            puts(linebuf.GetString());
            ++ diffs_output;
        }
        if (repo_entry->fetch_missing()) {
            goto try_more;
        }
        return diffs_output;
    }

    bool get_output(size_t idx)
    {
        const cppgit2::diff::delta & need_eeg_and_blockchain = (*diff)[idx];
        cppgit2::patch patch;
        auto status = need_eeg_and_blockchain.status();
        if (status == cppgit2::diff::delta::type::unmodified) {
            return false;
        }
        try {
            patch = cppgit2::patch(*diff, idx);
        } catch (cppgit2::git_exception &exc) {
            repo_entry->missing(exc, commit->id());
            return false;
        }
        output.clear();
        if (!output.can_append(patch.size(true, true, true))) {
            return false;
        }
        cppgit2::data_buffer patch_buffer = patch.to_buffer();
        git_buf const * patch_c_struct = patch_buffer.c_ptr(); 
        return output.append(patch_c_struct->ptr, patch_c_struct->size);
    }

    bool add_input(const cppgit2::diff::delta & need_eeg_and_blockchain, bool allow_empty)
    {
        auto old_file = need_eeg_and_blockchain.old_file();
        auto old_id = old_file.id();

        if (old_id.is_zero()) {
            if (!allow_empty) {
                return false;
            }
        } else {
            if (
                (old_file.mode() & 0777000) != (GIT_FILEMODE_BLOB & 0777000) ||
                old_file.flags() & (uint32_t)cppgit2::diff::delta::flag::binary
            ) {
                return false;
            }
        }

        static thread_local std::string old_path;
        old_path = old_file.path();
        size_t extra_size = file_name_start.size() + old_path.size() + file_name_end.size() + file_content_start.size() + file_content_end.size();
        size_t content_size = old_file.size();
        if (!more_input.can_append(extra_size + content_size)) {
            return false;
        }

        blob content;
        if (!old_id.is_zero()) {
            try {
                content = repo_entry->repository.lookup_blob(old_id);
            } catch (cppgit2::git_exception &exc) {
                repo_entry->missing(exc, commit->id());
                return false;
            }

            if (content_size == 0) {
                content_size = content.raw_size();
                if (!more_input.can_append(extra_size + content_size)) {
                    return false;
                }
            }
        }

        bool success = more_input.append(file_name_start);
        success &= more_input.append(old_path);
        success &= more_input.append(file_name_end);
        success &= more_input.append(file_content_start);
        if (!old_id.is_zero()) {
            success &= more_input.append(content.raw_contents(), content_size);
        }
        success &= more_input.append(file_content_end);
        if (!success) {
            throw std::logic_error("actual append failed after can_append succeeded; missing way to revert state to before partial append; maybe length_tracked_value could push/pop its state leaving more_input unneeded");
        }
        return true;
    }

    struct length_tracked_value
    {
        size_t max;
    #ifdef TOKENIZE
        size_t max_token_ids;
        size_t token_ids;
        rust::Box<Tokenizer> * tokenizer;
    #endif
        bool cut;
        std::string data;

        void clear()
        {
            data.clear();
            #ifdef TOKENIZE
            token_ids = 0;
            #endif
        }

        void to_append_to(length_tracked_value & other)
        {
            if (other.max) {
                max = other.max - other.data.size();
            } else {
                max = 0;
            }
            #ifdef TOKENIZE
            tokenizer = other.tokenizer;
            if (other.max_token_ids) {
                max_token_ids = other.max_token_ids - other.token_ids;
            } else{
                max_token_ids = 0;
            }
            #endif
            cut = other.cut;
        }

        bool set(void const * data, size_t length)
        {
            clear();
            return append(data, length);
        }

        bool append(std::string const & more_data)
        {
            return append(more_data.data(), more_data.size());
        }
        bool append(void const * more_data, size_t more_length)
        {
            if (max) {
                if (cut) {
                    if (max == data.size()) {
                        return false;
                    }
                } else if (data.size() + more_length > max) {
                    return false;
                }
            }
            #ifdef TOKENIZE
            if (max_token_ids) {
                size_t more_token_ids = token_length(more_data, more_length);
                if (token_ids + more_token_ids > max_token_ids) {
                    if (!cut) {
                        return false;
                    } else {
                        throw std::logic_error("cut with tokenization length limit not implemented");
                    }
                }
                token_ids += more_token_ids;
            } else
            #endif
            if ((cut & max) && data.size() + more_length > max) {
                more_length = max - data.size();
            }
            data.append((char const *)more_data, more_length);
            return true;
        }

        bool can_append(size_t more_length)
        {
            if (max && data.size() == max) {
                return false;
            }
            #ifdef TOKENIZE
            if (max_token_ids && token_ids == max_token_ids) {
                return false;
            }
            #endif
            if (cut) {
                return true;
            }
            if (max && data.size() + more_length > max) {
                return false;
            }
            #ifdef TOKENIZE
            if (max_token_ids && token_ids + more_length / 4 > max_token_ids) {
                return false;
            }
            #endif
            return true;
        }

        #ifdef TOKENIZE
        size_t token_length(void const * data, size_t length)
        {
            static thread_local rust::Box<Encoding> tokenization = rust::Box<Encoding>::from_raw(nullptr);
            tokenization = (*tokenizer)->encode(rust::cxxbridge1::String((char const *)data, length), false);
            return tokenization->get_ids().size();
        }
        #endif
    };

    std::default_random_engine rng;
    #ifdef TOKENIZE
    rust::Box<Tokenizer> tokenizer;
    #endif
    length_tracked_value input, more_input, output;
    std::string tokenizer_path;
    std::string input_start;
    std::string message_start;
    std::string message_end;
    std::string file_name_start;
    std::string file_name_end;
    std::string file_content_start;
    std::string file_content_end;
    std::string input_end;
    cppgit2::diff::options diff_options;
    repo_commits * repo_entry;
    cppgit2::commit const * commit;
    cppgit2::diff const * diff;
    std::vector<size_t> diff_idcs, diff_subidcs;
    std::unordered_set<size_t> skip_input_diff_idcs;
    std::unordered_map<cppgit2::oid, std::string, oid_hash> outputs;
    cppgit2::blob content;
};

int main(int argc, char **argv)
{
    unsigned int max_diffs_per_commit = 1;
    unsigned int max_commits_per_repo = 1;
    unsigned int seed = 0;
    unsigned int max_input_length = ~0; //256; //~0;
    unsigned int max_output_length = ~0; //256; //~0; //1024;
    unsigned int cycles_over_repos = 16; //2;//~0;
    #ifdef TOKENIZE
    bool lengths_are_tokenized = false;
    string tokenizer_path = "tokenizer.json";
    #endif
    bool cut_input = false;
    bool cut_output = false;
    string input_start = "";
    string message_start = "";
    string message_end = "";
    string file_name_start = "<pad>";
    string file_name_end = "<pad>";
    string file_content_start = "";
    string file_content_end = "";
    string input_end = "</s>";

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
        #ifdef TOKENIZE
        tokenizer_path,
        #endif
        input_start,
        message_start,
        message_end,
        file_name_start,
        file_name_end,
        file_content_start,
        file_content_end,
        input_end,
        diff_options.flags()
    );

    static thread_local unordered_map<string, repo_commits> repos;

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
                    throw std::logic_error("unimplemented: multimerge");
                }
    
                diff.find_similar(); // finds renames, copies. can have options passed.
                // diff.to_string()
                // diff.for_each([](const cppgit2::diff::delta &, float) {})
                // diff.print(diff:format, [](const diff
    
                outputter.init_commit(&repo_entry, &commit, &diff);
                if (outputter.process(max_diffs_per_commit)) {
                    commits_output ++;
                }
                // OPTIMIZE: this selection of inputs can be done by randomizing a range of integers and picking by integers
                // no need to enumerate them all [is this still relevant?]
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
                    // BUG: haven't implemented functions in tokenizer to find offset, so not cutting input
                    //if (input_size > max_input_length) {
                    //    input.resize(max_input_length);
                    //}
            }
        }
    }
}
