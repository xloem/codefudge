# download recent model to work off of
read hash cid < w3put.log
if ipfs --version
then
  ipfs config Addresses.Gateway /ip4/0.0.0.0/tcp/9001
  ipfs daemon &
  daemonpid=$!
  sleep 15
  ipfs get "$cid"
  kill $daemonpid
  cp -va "$hash"/fudge-* .
else
  uri=https://"${cid}".ipfs.dweb.link/
	wget --mirror "$uri"
  cp -va "${uri#*//}"fudge-* .
fi
