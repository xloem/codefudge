#!/usr/bin/env bash

# example_run_summarization.py is a direct copy from the adapter-transformers repository

MODEL=google/long-t5-tglobal-base
BATCH_SIZE=2
EPOCHS=1
MAX_IN_LEN=16384
MAX_OUT_LEN=16384

if ! [ -e .already_downloaded_model ]
then
    python3 example_run_summarization.py  --model_name_or_path "$MODEL" --do_train --output_dir output --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file test.json --train_adapter True --num_train_epochs 1 --max_train_samples 1 --max_source_length "$MAX_IN_LEN" --max_target_length "$MAX_OUT_LEN" && touch .already_downloaded_model
fi
TRANSFORMERS_OFFLINE=1 python3 example_run_summarization.py  --model_name_or_path "$MODEL" --do_train --output_dir output --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file test.json --train_adapter True --num_train_epochs "$EPOCHS" --max_source_length "$MAX_IN_LEN" --max_target_length "$MAX_OUT_LEN"
