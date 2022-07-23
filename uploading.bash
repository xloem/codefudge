w3 --version || {
	npm install --global @web3-storage/w3
	npm token
}
model=fudge-long-t5-tglobal-base
while true
do
	latest_checkpoint="$(ls -art "$model"/ | grep checkpoint | tail -n 1)"
	cp -va "$model"/"$latest_checkpoint" "$model"/"$model"
	rm "$model"/"$model"/*/pytorch_model_head.bin
	w3 put "$model"/"$model" | tee w3put.log
	git add w3put.log
	git commit -m "$(head -n 1 w3put.log)"
	git push
	rm -rf "$model"/"$model"
	sleep $((60*15))
done
