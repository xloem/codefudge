# download recent model to work off of
read hash cid < w3put.log
curl --head https://dweb.link/ipfs/"$cid" | tr -d '\r' | grep -i ^location: | {
	read location uri
	wget --mirror "$uri"
    cp -va "${uri#*//}"fudge-* .
}
