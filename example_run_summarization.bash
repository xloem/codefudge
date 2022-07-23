#!/usr/bin/env bash

# example_run_summarization.py is a direct copy from the adapter-transformers repository

MODEL=google/long-t5-tglobal-base
# 16 GB VRAM
BATCH_SIZE=1
GRAD_ACCUM=2
EPOCHS=1
MAX_OUT_LEN=2048
DATAFILE=test.json

if ! [ -e .already_downloaded_model ]
then
    python3 example_run_summarization.py  --model_name_or_path "$MODEL" --do_train --output_dir output --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file "$DATAFILE" --train_adapter True --num_train_epochs 1 --max_train_samples 1 --max_target_length "$MAX_OUT_LEN" && touch .already_downloaded_model
fi
if [ -e output/summarization/adapter_config.json ]
then
	TRANSFORMERS_OFFLINE=1 python3 example_run_summarization.py --load_adapter output/summarization --gradient_accumulation_steps "$GRAD_ACCUM" --model_name_or_path "$MODEL" --do_train --output_dir output --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file "$DATAFILE" --train_adapter True --num_train_epochs "$EPOCHS" --max_target_length "$MAX_OUT_LEN"
else
	TRANSFORMERS_OFFLINE=1 python3 example_run_summarization.py --gradient_accumulation_steps "$GRAD_ACCUM" --model_name_or_path "$MODEL" --do_train --output_dir output --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file "$DATAFILE" --train_adapter True --num_train_epochs "$EPOCHS" --max_target_length "$MAX_OUT_LEN"
fi
