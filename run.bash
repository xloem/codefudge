# install gh (which makes github push work) and w3, for checkpoint uploading
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
apt update
apt install gh

npm install --global @web3-storage/w3

# authenticate with user
#gh auth login
#w3 token

# install python dependencies
python3 -m pip install -U pip pynacl datasets sacremoses rouge-score git+https://github.com/xloem/adapter-transformers.git@longt5 torch

# download recent model to work off of
bash download.bash

# link data from google drive if present
if [ -e ../drive/MyDrive/codefudge/test.json ]
then
	ln -s ../drive/MyDrive/codefudge/test.json
fi

# start grooming
while true
do
	bash example_run_summarization.bash
done
