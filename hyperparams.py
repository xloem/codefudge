import git, json, logging, os, requests
from collections import defaultdict
logging.basicConfig(level=logging.DEBUG)
repo = git.Repo('.')

#GATEWAY='gateway.pinata.cloud'
GATEWAY='dweb.link'
PATH='w3put.log'
SUBURL='fudge-long-t5-tglobal-base/trainer_state.json'

def tree_lookup(tree, name):
    for subtree in tree.trees:
        if subtree.name == name:
            return subtree
    for blob in tree.blobs:
        if blob.name == name:
            return blob
def commit_lookup_path(commit, name):
    parts = name.split('/')
    item = commit.tree
    for part in parts:
        item = tree_lookup(item, part)
    return item

unvisited_commits = [repo.head.commit]
#visited_commits = {}
visited_commits = set()
blobs = {}

# due to doing this while engaging an inhibition, it does not associate blobs with the commits the introduced them

while len(unvisited_commits):
    commit = unvisited_commits.pop()
    #time = min(commit.committed_date, commit.authored_date)
    #if commit.hexsha in visited_commits:
    #    if visited_commits[commit.hexsha]['time'] > time:
    ##        visited_commits[commit.hexsha]['time'] = time
    #        visited_commits[commit.hexsha]['commit'] = commit
    #    continue
    if commit in visited_commits:
        continue
    time = commit.authored_date
    blob = commit_lookup_path(commit,PATH)
    unvisited_commits.extend(commit.parents)
    visited_commits.add(commit)
    if blob is not None:
        if blob in blobs:
            #import pdb; pdb.set_trace()
            if commit.authored_date < blobs[blob][1].authored_date:
                blobs[blob] = (blob, commit)
        else:
            blobs[blob] = (blob, commit)
    
print(f'found {len(visited_commits)} commits, {len(blobs)} blobs')

def blob2url(blob):
    content = blob.data_stream[-1].read()
    content = content.decode().strip()
    url = content.split(' ')[-1].replace('dweb.link', GATEWAY)
    return url

urls = [blob2url(blob) for blob in blobs]

try:
    with open('hyperparm_cache.json') as file:
        cache = json.load(file)
except:
    cache = {}
updated_cache = False
session = requests.Session()
try:
    for url, (blob, commit) in zip(urls, blobs.values()):
        if url not in cache and '://' in url:
            try:
                cache[url] = session.get(url + '/' + SUBURL).json()
                cache[url]['commit'] = commit.hexsha
            except requests.models.complexjson.JSONDecodeError:
                print(f'failed: {url}/{SUBURL}')
            except requests.exceptions.ConnectionError:
                print(f'could not connect to {GATEWAY}')
                break
finally:
    with open('hyperparm_cache.json.new', 'wt') as file:
        json.dump(cache, file)
    os.rename('hyperparm_cache.json.new', 'hyperparm_cache.json')

print(f'cache has {len(cache)} entries')

seen_logs = set()

data = []

for url, commit in zip(urls, blobs.values()):
    try:
        state = cache[url]
        #epoch_portion_per_step = state['num_train_epochs'] / state['max_steps']
        steps_per_epoch = state['max_steps'] / state['num_train_epochs']
        for prev, next in zip(state['log_history'][:-1], state['log_history'][1:]):
            id = (tuple(prev.values()), tuple(next.values()))
            if id in seen:
                continue
            seen.add(id)
            lr = (prev['learning_rate'] + next['learning_rate']) / 2
            loss_change_per_epoch = (prev['loss'] - next['loss']) * (next['step'] - prev['step']) / steps_per_epoch
            data.append((steps_per_epoch, lr, loss_change_per_epoch, commit.authored_date))
    except:
        continue
#example cache content:
# {
#     "https://gateway.pinata.cloud/ipfs/bafybeigr7wwnxzkxd4sg542fhhwxo4vqvo5wwsj2q4w6tfvbp2rckurc3y": {
#         "best_metric": null,
#         "best_model_checkpoint": null,
#         "epoch": 0.9668374746205163,
#         "global_step": 25000,
#         "is_hyper_param_search": false,
#         "is_local_process_zero": true,
#         "is_world_process_zero": true,
#         "log_history": [
#             {
#                 "epoch": 0.02,
#                 "learning_rate": 4.9033143829523916e-05,
#                 "loss": 2.1322,
#                 "step": 500
#             },
# ...
#             {
#                 "epoch": 0.97,
#                 "learning_rate": 1.6571914761960013e-06,
#                 "loss": 1.9421,
#                 "step": 25000
#             }
#         ],
#         "max_steps": 25857,
#         "num_train_epochs": 1,
#         "total_flos": 6.566108607989683e+16,
#         "trial_name": null,
#         "trial_params": null,
#         "commit": "59e5738ea14cf2a7e3c20ad86cb3174658118c68"
#     },
# ...
# }
