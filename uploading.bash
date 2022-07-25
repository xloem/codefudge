w3 --version || {
	npm install --global @web3-storage/w3
	npm token
}
model=fudge-long-t5-tglobal-base
while true
do
	rm -rf "$model"/"$model"
	latest_checkpoint="$(dirname "$(dirname "$(ls -t "$model"/checkpoint-*/*/pytorch_adapter.bin "$model"/*/pytorch_adapter.bin | head -n 1)")")"
	cp -va "$latest_checkpoint" "$model"/"$model"
	find "$model"/"$model" -name pytorch_model_head.bin | xargs rm
	w3 put "$model"/"$model" | tee w3put.log
	git pull --no-edit
	git add w3put.log
	git commit -m "$(head -n 1 w3put.log)"
	git push
	rm -rf "$model"/"$model"
	sleep $((60*15))
done
