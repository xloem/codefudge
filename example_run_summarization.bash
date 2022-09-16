#!/usr/bin/env bash

# example_run_summarization.py is a direct copy from the adapter-transformers repository

MODEL=google/long-t5-tglobal-base
EPOCHS=1
GRAD_ACCUM=$(((RANDOM % 12)+1))

# 16 GB VRAM
BATCH_SIZE=1
#MAX_OUT_LEN=2168 # without training embeddings, this worked for me for a few thousand steps
MAX_OUT_LEN=2048 # with training embeddings; 2164 failed after 5434; 2144 eventually failed on 15109MB
# 8 GB VRAM
#MAX_OUT_LEN=1152
# 2 GB VRAM
#MAX_OUT_LEN=80 # tradeoff between this and the input size

DATAFILE=test.json
OUTPUT_DIR=fudge-"${MODEL##*/}"

OPTIM=adafactor
WARMUP_STEPS=$((60*8))
LEARNING_RATE_CONTINUING=3.5e-06
LEARNING_RATE_STARTING=1.0e-04

LEARNING_RATE_CONTINUING=$(python3 <<< "print($LEARNING_RATE_CONTINUING * $GRAD_ACCUM * $BATCH_SIZE)")
LEARNING_RATE_STARTING=$(python3 <<< "print($LEARNING_RATE_STARTING * $GRAD_ACCUM * $BATCH_SIZE)")

if [ -e "$OUTPUT_DIR"/old_tokenizer ]
then
  TOKENIZER_PARAMS="--tokenizer_name $OUTPUT_DIR --old_tokenizer_path $OUTPUT_DIR/old_tokenizer"
else
  TOKENIZER_PARAMS="--tokenizer_name $OUTPUT_DIR"
fi

mkdir -p "$OUTPUT_DIR"
if ! [ -e "$OUTPUT_DIR"/.already_downloaded_model ]
then
    echo 'downloading groomed base model ...'
    python3 example_run_summarization.py --model_name_or_path "$MODEL" --do_train --output_dir "$OUTPUT_DIR/tmp" --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file "$DATAFILE" --train_adapter True --num_train_epochs 1 --max_train_samples 1 --max_target_length "$MAX_OUT_LEN" >/dev/null && touch "$OUTPUT_DIR"/.already_downloaded_model
    rm -rf "$OUTPUT_DIR"/tmp
fi
if [ -e "$OUTPUT_DIR"/summarization/adapter_config.json ]
then
	# shuffle the data
	echo 'shuffling data ...'
	REAL_DATAFILE=live-"$DATAFILE"
	cat "$DATAFILE" | shuf > "$REAL_DATAFILE"
	DATAFILE="$REAL_DATAFILE"

	echo continuing grooming of adapter ... machine learning models suffer much less than human beings when groomed for behaviors.
	TRANSFORMERS_OFFLINE=1 python3 example_run_summarization_plus_embeddings.py --learning_rate "$LEARNING_RATE_CONTINUING" --optim "$OPTIM" --warmup_steps "$WARMUP_STEPS" $TOKENIZER_PARAMS --load_adapter "$OUTPUT_DIR"/summarization --gradient_accumulation_steps "$GRAD_ACCUM" --model_name_or_path "$MODEL" --do_train --output_dir "$OUTPUT_DIR" --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file "$DATAFILE" --train_adapter True --num_train_epochs "$EPOCHS" --max_target_length "$MAX_OUT_LEN"
else
	if ! [ -e "$OUTPUT_DIR"/tokenizer ]
	then
		echo "extending tokenizers ..."
		echo "if you have a tokenizer you would like to use, put it in $OUTPUT_DIR/tokenizer"
		python3 extend_tokenizer.py
	fi
	# shuffle the data and sort by length
	echo 'sorting data ...'
	REAL_DATAFILE=live-"$DATAFILE"
	cat "$DATAFILE" | shuf | awk '{ print length, $0 }' | sort -n -s | cut -d" " -f2- > "$REAL_DATAFILE"
	DATAFILE="$REAL_DATAFILE"

	echo grooming a new adapter ... machine learning models suffer much less than human beings when groomed for behaviors.
	TRANSFORMERS_OFFLINE=1 python3 example_run_summarization_plus_embeddings.py --adapter_config pfeiffer+inv --learning_rate "$LEARNING_RATE_STARTING" --optim "$OPTIM" --warmup_steps "$WARMUP_STEPS" $TOKENIZER_PARAMS --gradient_accumulation_steps "$GRAD_ACCUM" --model_name_or_path "$MODEL" --do_train --output_dir "$OUTPUT_DIR" --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file "$DATAFILE" --train_adapter True --num_train_epochs "$EPOCHS" --max_target_length "$MAX_OUT_LEN"
fi && if [ -e "$OUTPUT_DIR/old_tokenizer" ]
then
  mv -v "$OUTPUT_DIR"/old_tokenizer "$OUTPUT_DIR"/old_tokenizer.used-"$(date --iso)"
fi
