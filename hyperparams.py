import git, json, logging, os, requests, sys
import math, datetime, shutil
import tqdm
from collections import defaultdict
#logging.basicConfig(level=logging.DEBUG)
repo = git.Repo('.')

#GATEWAY='gateway.pinata.cloud'
GATEWAY='dweb.link'
PATH='w3put.log'
SUBURL='fudge-long-t5-tglobal-base/trainer_state.json'
DATALEN = 51712
EARLIEST = datetime.datetime.now().timestamp() - 60 * 60 * 24 * 28
GRAD_ACCUM_MAX = None #4
SUBDIVISIONS = 1

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
    
print(f'found {len(visited_commits)} commits, {len(blobs)} blobs', file=sys.stderr)

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
    for url, (blob, commit) in zip(tqdm.tqdm(urls, desc='ipfs'), blobs.values()):
        if url not in cache and '://' in url:
            try:
                cache[url] = session.get(url + '/' + SUBURL).json()
                cache[url]['commit'] = commit.hexsha
            except requests.models.complexjson.JSONDecodeError:
                print(f'failed: {url}/{SUBURL}', file=sys.stderr)
            except requests.exceptions.ConnectionError:
                print(f'could not connect to {GATEWAY}', file=sys.stderr)
                break
finally:
    with open('hyperparm_cache.json.new', 'wt') as file:
        json.dump(cache, file)
    os.rename('hyperparm_cache.json.new', 'hyperparm_cache.json')

print(f'cache has {len(cache)} entries', file=sys.stderr)

seen_logs = set()

data = []

for url, (blob, commit) in zip(tqdm.tqdm(urls, desc='train states'), blobs.values()):
    #try:
        if url not in cache:
            continue
        state = cache[url]
        #epoch_portion_per_step = state['num_train_epochs'] / state['max_steps']
        #if int(state['max_steps'] / DATALEN) * DATALEN != state['max_steps']:
        #    continue
        steps_per_epoch = state['max_steps'] / state['num_train_epochs']
        log_history = state['log_history']
        if 'total_flos' in log_history[-1]:
            tail = log_history.pop()
            samples_per_step = int(tail['train_samples_per_second'] / tail['train_steps_per_second'] + 0.5)
            samples_per_second = tail['train_samples_per_second']
            samples_per_epoch = tail['step'] * samples_per_step // tail['epoch']
        for prev, next in zip(log_history[:-1], log_history[1:]):
            id = (tuple(prev.values()), tuple(next.values()))
            if id in seen_logs:
                continue
            seen_logs.add(id)
            lr = (prev['learning_rate'] + next['learning_rate']) / 2
            loss = (prev['loss'] + next['loss']) / 2
            step = (prev['step'] + next['step']) / 2
            loss_change_per_epoch = (prev['loss'] - next['loss']) * (next['step'] - prev['step']) / steps_per_epoch
            entry = (steps_per_epoch, lr, loss, loss_change_per_epoch, commit.authored_date, step)
            data.append(entry)
    #except:
    #    continue
#def sort_key(elem):
#    steps_per_epoch, lr, loss, loss_change_per_epoch, authored_date, step = elem
#
#data.sort(key = l
try:
    shutil.rmtree('hyperp-dbg')
except:
    pass
os.mkdir('hyperp-dbg')
buckets = defaultdict(list)
for idx, (steps_per_epoch, lr, loss, loss_change_per_epoch, authored_date, step) in enumerate(data):
    print(idx, steps_per_epoch, lr, loss_change_per_epoch, authored_date)
    grad_accum = round(DATALEN / steps_per_epoch)
    scaled_lr = lr / grad_accum
    lr_exp = 10 ** int(math.log(scaled_lr) / math.log(10) - 1.5)
    rounded_lr = round(scaled_lr / lr_exp * SUBDIVISIONS) * lr_exp / SUBDIVISIONS
    #rounded_steps_per_epoch = round(steps_per_epoch / 1000) * 1000
    if authored_date < EARLIEST:
        continue
    if GRAD_ACCUM_MAX is not None and grad_accum > GRAD_ACCUM_MAX:
        continue
    with open(f'hyperp-dbg/{rounded_lr}-{grad_accum}', 'at') as f:
        f.write(f'{idx} {lr} {loss} {loss_change_per_epoch} {authored_date}\n')
    buckets[(rounded_lr, grad_accum)].append((idx, loss, loss_change_per_epoch, authored_date))
options = [(rounded_lr, grad_accum, elems) for (rounded_lr, grad_accum), elems in buckets.items()]
print('\n')
print('lr', 'grad_accum', '# items', 'loss_change_per_epoch')
options.sort(key = lambda option: sum((loss_change_per_epoch for idx, loss, loss_change_per_epoch, authored_date in option[2]))/len(option[2]))
for rounded_lr, grad_accum, elems in options:#[-16:]:
    print(rounded_lr, grad_accum, f'{len(elems)}x',
            sum((loss_change_per_epoch for idx, loss, loss_change_per_epoch, authored_date in elems))/len(elems),
            datetime.datetime.fromtimestamp(sum((authored_date for idx, loss, loss_change_per_epoch, authored_date in elems))/len(elems)).isoformat(),
            #*(idx for idx, loss, loss_change_per_epoch, authored_date in elems)
    )
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
