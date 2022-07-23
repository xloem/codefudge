npm install --global @web3-storage/w3
model=fudge-long-t5-tglobal-base
while true
do
	latest_checkpoint="$(ls -art "$model"/ | grep checkpoint | tail -n 1)"
	w3 put "$model"/"$latest_checkpoint" | tee w3put.log
	git add w3put.log
	git commit -m "$(head -n 1 w3put.log)"
	git push
	sleep $((60*15))
done
