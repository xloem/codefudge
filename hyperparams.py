import git, json, logging, requests
logging.basicConfig(level=logging.DEBUG)
repo = git.Repo('.')

GATEWAY='gateway.pinata.cloud'
PATH='w3put.log'
SUBURL='fudge-long-t5-tglobal-base/trainer_state.json'

def tree_lookup(tree, name):
    for tree in tree.trees:
        if tree.name == name:
            return tree
    for blob in tree.blobs:
        if blob.name == name:
            return blob
def commit_lookup_path(commit, name):
    parts = name.split('/')
    item = commit.tree
    for part in parts:
        item = tree_lookup(item, part)
    return item

unvisited_commits = set([repo.head.commit])
#visited_commits = {}
visited_commits = set()
blobs = set()

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
    blob = commit_lookup_path(commit,PATH)
    unvisited_commits.update(commit.parents)
    visited_commits.add(commit)
    if blob is not None:
        blobs.add(blob)
    #visited_commits[commit.hexsha] = {
    #    'commit': commit,
    #    'blob': blob,
    #    'time': time,
    #}
    
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
for idx, url in enumerate(urls):
    if url not in cache and '://' in url:
        try:
            updated_cache = True
            cache[url] = requests.get(url + '/' + SUBURL).json()
        except:
            print(f'failed: {url}/{SUBURL}')
if updated_cache:
    with open('hyperparm_cache.json.new', 'wt') as file:
        json.dump(cache, file)
    os.rename('hyperparm_cache.json.new', 'hyperparm_cache.json')

print(f'cache has {len(cache)} entries')
