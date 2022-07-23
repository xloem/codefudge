# start repos cloning and generating commit files
{
	echo https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git linux
	echo https://github.com/TheAlgorithms/Python TheAlgorithms_Python
	echo https://github.com/OGRECave/ogre ogre
	echo https://github.com/graphistry/pygraphistry graphistry
	echo https://github.com/opencog/opencog opencog
} | while read repo name
do {
	cd ..
	git clone "$repo" name >/dev/null 2>&1
	cd name
	../codefudge/hist.bash "$name" >/dev/null 2>&1
} & done

# install gh (which makes github push work) and w3, for checkpoint uploading
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
apt update
apt install gh

npm install --global @web3-storage/w3

# authenticate with user
gh auth login
w3 token

# install python dependencies
python3 -m pip install datasets sacremoses rouge-score git+https://github.com/xloem/adapter-transformers.git@longt5 torch

# start generating test.json from commits
while true
do
	python3 hist2json.py  >/dev/null
	sleep 60
done &

# start uploading
{
	git config user.email 0xloem@gmail.com
	git config user.name 'John Doe / Karl Semich'
	sleep 60
	bash uploading.bash
} &

# download recent model to work off of
read hash cid < w3put.log
curl --head https://dweb.link/ipfs/"$b" | tr -d '\r' | grep -i ^location: | {
	read location uri
	wget --mirror "$uri"
	cp -va *.ipfs.dweb.link/fudge-* 
}

# start grooming
while true
do
	bash example_run_summarization.bash
done
