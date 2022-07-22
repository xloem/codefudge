#!/usr/bin/env bash

# example_run_summarization.py is a direct copy from the adapter-transformers repository

MODEL=google/long-t5-tglobal-base
BATCH_SIZE=1
EPOCHS=16
MAX_IN_LEN=128
MAX_OUT_LEN=128

if ! [ -e .already_downloaded_model ]
then
    python3 -c "import transformers; transformers.pipeline('summarization', '$MODEL')"
    touch .already_downloaded_model
fi
TRANSFORMERS_OFFLINE=1 python3 example_run_summarization.py  --model_name_or_path "$model" --do_train --output_dir output --per_device_train_batch_size="$BATCH_SIZE" --overwrite_output_dir --predict_with_generate --train_file test.json --train_adapter True --num_train_epochs "$EPOCHS" --max_source_length "$MAX_IN_LEN" --max_target_length "$MAX_OUT_LEN"
