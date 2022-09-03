#include <algorithm>
#include <cstdio>
#include <deque>
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

#define cout cerr
#include "progressbar/include/progressbar.hpp"
#undef cout

#ifdef TOKENIZE
#include "tinytokenizers.rs.h"
#endif

// C++ standard library hashing functor for cppgit2 oids
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

// C++ standard library hashing functor for cppgit2 objects
struct object_hash
{
    template <class T>
    std::size_t operator() (const T & object) const {
        return oid_hash()(object.id());
    }
};

// rapidjson encoding for binary data
template<typename CharType = char>
struct InclusiveUTF8 : public rapidjson::UTF8<CharType>
{
    // Encode malformed input as Latin-1 Supplement, which preserves raw values.
    template <typename InputStream>
    static bool Decode(InputStream &is, unsigned* codepoint)
    {
        typename InputStream::Ch c = is.Peek();
        if (rapidjson::UTF8<CharType>::Decode(is, codepoint)) {
            return true;
        } else {
            if (c < 0x100) {
                *codepoint = static_cast<unsigned char>(c);
                return true;
            } else {
                return false;
            }
        }
    }
};

// rapidjson decoding that escapes binary special chars
template<typename CharType = char>
struct ExtraEscapedUTF8 : public rapidjson::UTF8<CharType>
{
    // Encode Latin-1 Supplement C1 Controls with string escaping
    template<typename OutputStream>
    static void EncodeUnsafe(OutputStream& os, unsigned codepoint)
    {
        static const typename OutputStream::Ch hexDigits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
        if (codepoint >= 0x80 && codepoint < 0xa0) {
            os.PutUnsafe('\\');
            os.PutUnsafe('u');
            os.PutUnsafe('0');
            os.PutUnsafe('0');
            os.PutUnsafe(hexDigits[(codepoint >> 4)     ]);
            os.PutUnsafe(hexDigits[(codepoint     ) & 15]);
        } else {
            rapidjson::UTF8<CharType>::EncodeUnsafe(os, codepoint);
        }
    }
};

struct repo_commits
{
    std::string path;
    cppgit2::repository repository;
    std::vector<cppgit2::oid> commits;
    //std::unordered_set<cppgit2::oid, oid_hash> commits;
    //std::unordered_map<cppgit2::oid, std::vector<cppgit2::oid>> 
    std::vector<cppgit2::reference> references;
    std::vector<std::pair<cppgit2::remote, cppgit2::refspec>> remote_fetchspecs;
    std::unordered_map<std::string, std::unordered_set<cppgit2::oid, oid_hash>> missing_objects_by_remote_name;
    std::unordered_map<cppgit2::oid, std::string, oid_hash> remote_names_by_missing_commit;
    std::unordered_map<cppgit2::oid, std::string, oid_hash> remote_names_by_nonremote_oids;


    repo_commits(char const * path)
    : path(path),
      repository(cppgit2::repository::open(path))
    {
        std::cerr << "Loading commits for " << path << std::endl;
        std::vector<cppgit2::reference> references;
        for (
            reference_iter.init(repository);
            reference_iter;
            ++ reference_iter
        ) {
            references.push_back(*reference_iter);
        }
        progressbar bar(references.size());
        std::deque<std::pair<cppgit2::oid, cppgit2::oid>> commit_queue;
        std::unordered_set<cppgit2::oid, oid_hash> visited_commits;
        for (auto & reference : references) {
            bar.update();
            auto ref_object = ref_oid_to_commit_object(reference.resolve().target());
            auto ref_id = ref_object.id();
            commit_queue.emplace_back(ref_id, ref_id);
            while (!commit_queue.empty()) {
                auto tip_oid = commit_queue.back().first;
                auto commit_oid = commit_queue.back().second;
                auto it_success_pair = visited_commits.emplace(commit_oid);
                commit_queue.pop_back();
                if (!it_success_pair.second) {
                    continue;
                }
                try {
                    cppgit2::object object = ref_oid_to_commit_object(commit_oid);
                    cppgit2::commit commit = object.as_commit();
                    commits.push_back(commit_oid);
                    size_t parent_count = commit.parent_count();
                    for (size_t parent_idx = 0; parent_idx < parent_count; ++ parent_idx) {
                        commit_queue.emplace_back(tip_oid, commit.parent_id(parent_idx));
                    }
                } catch (cppgit2::git_exception const &exc) {
                    visited_commits.erase(commit_oid);
                    if (!missing(exc, tip_oid)) {
                        commit_queue.emplace_back(tip_oid, commit_oid);
                        fetch_missing();
                    } else {
                        commit_queue.emplace_front(tip_oid, commit_oid);
                    }
                }
            }
        }
        //static cppgit2::revwalk revwalk;
        //revwalk.reset();
        
        //auto revwalk = repository.create_revwalk();
        //revwalk.push_glob("*");
        //while (!revwalk.done()) {
        //    commits.push_back(revwalk.next());
        //}
    }

    std::string remote_name(cppgit2::oid const & commit)
    {
        std::cerr << "Looking for remote for " << commit.to_hex_string() << " ..." << std::endl;
        for (
            remote_branch_iter.init(repository, cppgit2::branch::branch_type::remote);
            remote_branch_iter;
            ++ remote_branch_iter
        ) {
            auto branch = *remote_branch_iter;
            auto branch_tip = branch.resolve().target();
            assert(branch.resolve().name().compare(0, 5, "refs/") == 0); // below code assumes a refs/ prefix, remove prefix if not included in api
            branch_tip = ref_oid_to_commit_object(branch_tip).id();
            if (branch_tip == commit || repository.is_descendant_of(branch_tip, commit)) {
                std::cerr << "Found remote branch " << branch.name() << " containing " << commit.to_hex_string() << std::endl;
                return branch_remote_name(branch.name());
            }
        }
        std::cerr << "Had some trouble finding which remote " << commit.to_hex_string() << " came from ..." << std::endl;

        {
            // i may have here implemented this comment block
        // not found on any remote branches; it may be a remote tag the branch of which was rebased away. so, look for a shallow merge base.
        // note: the mapping of branches to tags could be cached. the reason to do it when encountered would be because it could be harder to detect if a tip is on a remote if it has already been fetched.
        //              can likely create a remote ref with `git update-ref [-m reason] ref newvalue`, also takes stdin.
        //              can also be created with repository.create_reference(name, id, overwrite_flag, log_message)
        //              fetchspecs can also be specified to include tags
        //              tags for a remote shown by `git ls-remote --tags remote`
        //              remote refs can also be shown by remote.reference_advertisement_list(). gives oid and string name. tags likely start with 'refs/tags/'
            std::string found_remote, found_remote_branch;
            auto remote_list = repository.remote_list();
            progressbar bar(remote_list.count());
            for (auto const & remote_name : remote_list) {
                auto remote = repository.lookup_remote(remote_name);
                bar.update();
                try {
                    remote.connect(cppgit2::connection_direction::fetch);
                    for (auto & remote_ref : remote.reference_advertisement_list()) {
                        auto remote_refname = remote_ref.name();
                        auto remote_refid = remote_ref.id();
                        bool just_found = false;
                        assert(remote_refname.compare(0, 5, "refs/") == 0);
                        if (remote_refid == commit || repository.is_descendant_of(remote_refid, commit)) {
                            found_remote = remote_refname;
                            just_found = true;
                        }
                        if (just_found || remote_refname.compare(0, 10, "refs/tags/") == 0) {
                            auto ref_subpath = remote_refname.substr(4);
                            auto new_branchpath = "refs/remotes/" + remote_name + ref_subpath;
                            repository.create_reference(new_branchpath, remote_refid, false, "Created remote branch " + new_branchpath + " to track more commits");
                            if (just_found) {
                                found_remote_branch = new_branchpath;
                            }
                        }
                    }
                    remote.disconnect();
                } catch (...) {
                    remote.disconnect();
                    throw;
                }
            }
            if (!found_remote.empty()) {
                std::cerr << "Found " << commit.to_hex_string() << " on " << found_remote << " now as " << found_remote_branch <<  std::endl;
                return remote_name(commit); // this could be just `return found_remote;`. the recursion here enforces that the caching works, crashing if it does not.
            }
        }

        static thread_local std::vector<cppgit2::oid> non_remote_references;
        non_remote_references.clear();
        for (
            reference_iter.init(repository);
            reference_iter;
            ++ reference_iter
        ) {
            auto branch = *reference_iter;
            if (branch.is_remote()) {
                continue;
            }
            auto branch_tip = branch.resolve().target();
            branch_tip = ref_oid_to_commit_object(branch_tip).id();
            if (branch_tip == commit || repository.is_descendant_of(branch_tip, commit)) {
                auto entry = remote_names_by_nonremote_oids.find(branch_tip);
                if (entry != remote_names_by_nonremote_oids.end()) {
                    std::cerr << "Found it before: " << entry->second << std::endl;
                    return entry->second;
                }
                non_remote_references.push_back(branch_tip);
            }
        }
        // look for a shallow merge base.
        std::string best_branch;
        cppgit2::oid best_base;
        cppgit2::oid mapped_reference;
        for (
            remote_branch_iter.init(repository, cppgit2::branch::branch_type::remote);
            remote_branch_iter;
            ++ remote_branch_iter
        ) {
            auto branch = *remote_branch_iter;
            auto branch_tip = branch.resolve().target();
            branch_tip = ref_oid_to_commit_object(branch_tip).id();
            for (auto & reference : non_remote_references) {
                cppgit2::oid merge_base;
                try {
                    merge_base = repository.find_merge_base(branch_tip, reference);
                } catch (cppgit2::git_exception &exc) {
                    // no merge base found
                    continue;
                }
                if (best_branch.empty() || repository.is_descendant_of(merge_base, best_base)) {
                    best_base = merge_base;
                    best_branch = branch.name();
                    mapped_reference = reference;
                }
            }
        }
        // an exception here would imply failure in finding a missing object in remotes
        std::cerr << "Looks like " << commit.to_hex_string() << " is nearest to " << best_branch << endl;
        std::string remote_name = branch_remote_name(best_branch);
        remote_names_by_nonremote_oids[mapped_reference] = remote_name;
        return remote_name;
    }

    std::string branch_remote_name(std::string const & branch_name)
    {
        if (remote_fetchspecs.empty()) {
            std::cerr << "Loading remotes for " << path << std::endl;
            auto remote_names = repository.remote_list();
            progressbar bar(remote_names.count());
            for (auto const & remote_name : remote_names) {
                cppgit2::remote remote = repository.lookup_remote(remote_name);
                for (auto const & fetch_refspec : remote.fetch_refspec()) {
                    cppgit2::refspec fetchspec = cppgit2::refspec::parse(fetch_refspec, true);
                    remote_fetchspecs.emplace_back(std::move(remote), std::move(fetchspec));
                }
                bar.update();
            }
        }
        for (auto & remote_fetchspec : remote_fetchspecs) {
            auto & remote = remote_fetchspec.first;
            auto & fetchspec = remote_fetchspec.second;
            if (fetchspec.destination_matches_reference(branch_name)) {
                std::cerr << "Mapped remote branch " << branch_name << " to remote " << remote.name() << std::endl;
                return remote.name();
            }
        }
        throw std::logic_error(branch_name + " is a remote branch but no remote fetchspecs matched");
    }

    bool missing(cppgit2::oid const & missing_object, cppgit2::oid const & commit)
    {
        // the remote containing branch should have the missing object
        std::string remote_name;
        auto name_it = remote_names_by_missing_commit.find(commit);
        if (name_it != remote_names_by_missing_commit.end()) {
            remote_name = name_it->second;
        } else {
            remote_name = this->remote_name(commit);
            remote_names_by_missing_commit[commit] = remote_name;
        }
        cerr << missing_object.to_hex_string() << " is missing; " << remote_name << " should contain it from " << commit.to_hex_string() << endl;
        return missing_objects_by_remote_name[remote_name].insert(missing_object).second;
    }

    bool missing(std::string const & missing_object, cppgit2::oid const & commit)
    {
        return missing(cppgit2::oid(missing_object), commit);
    }

    bool missing(cppgit2::git_exception const &exc, cppgit2::oid const & commit)
    {
        string msg = exc.what();
        // todo: check exception to verify it is a missing object
        size_t end = msg.rfind(')');
        size_t start = msg.rfind('(', end);
        if (end == std::string::npos || start == std::string::npos) {
            throw exc;
        }
        start += 1;
        return missing(msg.substr(start, end - start), commit);
    }

    bool fetch_missing()
    {
        //remote_names_by_missing_commits.clear();
        
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
    
    struct reference_iterator {
        reference_iterator()
        : c_ptr(0), c_ref(0)
        { }
        void init(cppgit2::repository & repo)
        {
            git_reference_iterator_new(&c_ptr, const_cast<git_repository*>(repo.c_ptr()));
            ++(*this);
        }
        cppgit2::reference operator*()
        {
            return {c_ref};
        }
        void operator++()
        {
            if (git_reference_next(&c_ref, c_ptr) != 0) {
                c_ref = 0;
            }
        }
        operator bool() {
            return c_ref;
        }
        void free()
        {
            if (c_ptr) {
                git_reference_iterator_free(c_ptr);
                c_ptr = 0;
                c_ref = 0;
            }
        }
        ~reference_iterator()
        {
            free();
        }
        git_reference_iterator *c_ptr;
        git_reference *c_ref;
    } reference_iter;

    cppgit2::object ref_oid_to_commit_object(cppgit2::oid const & oid)
    {
        cppgit2::object object = repository.lookup_object(oid, cppgit2::object::object_type::any);
        while (object.type() == cppgit2::object::object_type::tag) {
            object = object.as_tag().target();
        }
        return object;
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
        string output_start,
        string output_end,
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
      input_end(input_end),
      output_start(output_start),
      output_end(output_end)
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
            const cppgit2::diff::delta & outer_need_eeg_and_blockchain = (*diff)[idx];

            if (outer_need_eeg_and_blockchain.status() == cppgit2::diff::delta::type::unmodified) {
                continue;
            }
            
            more_input.clear();
            if (!add_input(outer_need_eeg_and_blockchain, true, true)) {
                skip_input_diff_idcs.insert(idx);
                continue;
            }

            if (!get_output(idx)) {
                continue;
            }

            std::shuffle(diff_subidcs.begin(), diff_subidcs.end(), rng);
            while ("missing objects") {
                repo_entry->fetch_missing();

                for (size_t diff_subidx = 0; more_input.can_append(file_name_start.size() + file_name_end.size() + file_content_start.size() + file_content_end.size() + 16 + input_end.size()) && diff_subidx < diff_subidcs.size(); ++ diff_subidx)
                {
                    size_t subidx = diff_subidcs[diff_subidx];
                    if (idx == subidx || skip_input_diff_idcs.count(subidx)) {
                        continue;
                    }
                    const cppgit2::diff::delta & inner_need_eeg_and_blockchain = (*diff)[subidx];
                    add_input(inner_need_eeg_and_blockchain, false, false);
                }
               
                if (repo_entry->fetch_missing()) {
                    more_input.clear();
                    bool initial_success = add_input(outer_need_eeg_and_blockchain, true, false);
                    assert(initial_success);
                    continue;
                } else {
                    break;
                }
            }

            more_input.append(input_end);

            static thread_local rapidjson::StringBuffer linebuf(0, (input.max > 0 && input.max < ~0 ? input.max : 1024 * 1024) + (output.max > 0 && output.max < ~0 ? output.max : 1024 * 1024) + 64);
            static thread_local rapidjson::Writer<rapidjson::StringBuffer, InclusiveUTF8<>, ExtraEscapedUTF8<>> lineout;
            linebuf.Clear();
            lineout.Reset(linebuf);
            lineout.StartObject();
            more_input.data = input.data + more_input.data;
            more_input.fix_unicode(); output.fix_unicode();
            lineout.String("input", 5); lineout.String(more_input.data.data(), more_input.data.size());
            lineout.String("label", 5); lineout.String(output.data.data(), output.data.size());
            auto commit_id = commit->id();
            //auto remote_name = repo_entry->remote_name(commit_id);
            //if (!remote_name.empty()) {
            //    lineout.String("remote", 6); lineout.String(remote_name.data(), remote_name.size());
            //}
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
        output.set(output_start.data(), output_start.size());
        if (!output.can_append(patch.size(true, true, true))) {
            return false;
        }
        cppgit2::data_buffer patch_buffer = patch.to_buffer();
        git_buf const * patch_c_struct = patch_buffer.c_ptr(); 
        bool result = output.append(patch_c_struct->ptr, patch_c_struct->size);
        result &= output.append(output_end);
        return result;
    }

    bool add_input(const cppgit2::diff::delta & need_eeg_and_blockchain, bool allow_empty, bool only_unique)
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
        size_t extra_size = file_name_start.size() + old_path.size() + file_name_end.size() + file_content_start.size() + file_content_end.size() + file_content_end.size() + input_end.size();
        size_t content_size = old_file.size();
        if (!more_input.can_append(extra_size + content_size)) {
            return false;
        }

        blob content;
        if (!old_id.is_zero()) {
            if (only_unique && visited_oid_hashes.count(oid_hash()(old_id))) {
                return false;
            }
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
        if (!success && !more_input.cut) {
            throw std::logic_error("actual append failed after can_append succeeded; missing way to revert state to before partial append; maybe length_tracked_value could push/pop its state leaving more_input unneeded");
        }
        if (allow_empty && !old_id.is_zero()) {
            visited_oid_hashes.insert(oid_hash()(old_id));
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

        /*
        void reserve_for(void const * more_data, size_t length)
        {
            if (max) {
                assert(max >= length + data.size());
                max -= length;
            }
            #ifdef TOKENIZE
            if (max_token_ids) {
                size_t more_token_ids = token_length(more_data, more_length);
                assert(max_token_ids >= more_token_ids + token_ids);
                max_token_ids -= more_token_ids;
            }
            #endif
        }
        */

        bool set(void const * more_data, size_t length)
        {
            clear();
            return append(more_data, length);
        }

        bool append(std::string const & more_data)
        {
            return append(more_data.data(), more_data.size());
        }
        bool append(void const * more_data, size_t more_length)
        {
            if (max) {
                assert(max >= data.size());
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
            if ((cut & (bool)max) && data.size() + more_length > max) {
                more_length = max - data.size();
            }
            data.append((char const *)more_data, more_length);
            assert(!max || max >= data.size());
            return true;
        }

        /*
        bool is_cut()
        {
            assert(!max || max >= data.size());
            assert(!max_token_ids || max_token_ids >= token_ids);
            return (max && max == data.size()) || (max_token_ids && max_token_ids == token_ids);
        }
        */

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

        void char2unicode(uint8_t codepoint, char & byte_1, char & byte_2)
        {
            byte_1 = 0b11000000 | (codepoint >> 6);
            byte_2 = 0b10000000 | (codepoint & 0b111111);
        }

        void fix_unicode()
        {
            if (data.size() > 0) {
                char byte_1, byte_2;
                if ((data[data.size() - 1] & 0b11000000) == 0b11000000) {
                    // last byte would look like a unicode character that goes off the buffer
                    char2unicode(data[data.size() - 1], byte_1, byte_2);
                    data[data.size() - 1] = byte_1;
                    data += byte_2;
                } else if (data.size() > 1) {
                    if ((data[data.size() - 2] & 0b11100000) == 0b11100000) {
                        // second to last byte would look like a unicode character that goes off the buffer
                        char2unicode(data[data.size() - 2], byte_1, byte_2);
                        data[data.size() - 2] = byte_1;
                        data.insert(data.end() - 1, byte_2);
                        fix_unicode();
                    } else if (data.size() > 2) {
                        if ((data[data.size() - 3] & 0b11110000) == 0b11110000) {
                            // third to last byte would look like a unicode character that goes off the buffer
                            char2unicode(data[data.size() - 3], byte_1, byte_2);
                            data[data.size() - 3] = byte_1;
                            data.insert(data.end() - 2, byte_2);
                            fix_unicode();
                        }
                    }
                }
            }
        }
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
    std::string output_start;
    std::string output_end;
    cppgit2::diff::options diff_options;
    repo_commits * repo_entry;
    cppgit2::commit const * commit;
    cppgit2::diff const * diff;
    std::vector<size_t> diff_idcs, diff_subidcs;
    std::unordered_set<size_t> skip_input_diff_idcs;
    std::unordered_map<cppgit2::oid, std::string, oid_hash> outputs;
    cppgit2::blob content;
    static std::unordered_multiset<size_t> visited_oid_hashes;
};
std::unordered_multiset<size_t> output_manager::visited_oid_hashes;

int main(int argc, char **argv)
{
    unsigned int max_diffs_per_commit = 1;
    unsigned int max_commits_per_repo = 1;
    unsigned int seed = 0;
    unsigned int max_input_length = 1024 * 128;//1024 * 16; //256; //~0;
    unsigned int max_output_length = ~0; //256; //~0; //1024;
    unsigned int cycles_over_repos = 16; //2;//~0;
    #ifdef TOKENIZE
    bool lengths_are_tokenized = false;
    string tokenizer_path = "tokenizer.json";
    #endif
    bool cut_input = true;
    bool cut_output = false;
    string input_start = "";
    string message_start = "";
    string message_end = "";
    string file_name_start = "<pad>";
    string file_name_end = "<pad>";
    string file_content_start = "";
    string file_content_end = "";
    string input_end = "</s>";
    string output_start = "";
    string output_end = "</s>";

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
        output_start,
        output_end,
        diff_options.flags()
    );

    static thread_local unordered_map<string, repo_commits> repos;

    for (unsigned int repo_cycle = 0; repo_cycle < cycles_over_repos; ++ repo_cycle)
    {
        for (char **pathptr = &argv[1]; pathptr != &argv[argc]; ++ pathptr)
        {
            try {
                if (!repos.count(*pathptr)) {
                    repos.emplace(*pathptr, *pathptr);
                }
                repo_commits & repo_entry = repos.at(*pathptr);
                auto & repository = repo_entry.repository;
                auto & commit_oids = repo_entry.commits;
    
                if (commit_oids.size() < max_commits_per_repo * cycles_over_repos) {
                    std::cerr << "Skipping because it has few commits: " << *pathptr << std::endl;
                    continue;
                }
    
                // this could simply select a random index repeatedly
                shuffle(commit_oids.begin(), commit_oids.end(), rng);
        
                int commits_output = 0;
                for (int commit_idx = 0; commits_output < max_commits_per_repo && commit_idx < commit_oids.size(); ++ commit_idx) while ("missing objects") try {
                    cppgit2::commit commit;
                    commit = repository.lookup_commit(commit_oids[commit_idx]);
        
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
                    break;
                } catch (cppgit2::git_exception &exc) {
                    repo_entry.missing(exc, commit_oids[commit_idx]);
                    repo_entry.fetch_missing();
                    continue;
                }
            } catch (std::exception & e) {
                std::cerr << *pathptr << ": " << e.what() << std::endl;
                throw;
            }
        }
    }
}
