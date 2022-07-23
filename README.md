# CodeFudge

## Ingredients:

1. one git repository of your choosing, with the same parent folder as this one
2. bash, python, pytorch, datasets, rouge-score and git+https://github.com/xloem/adapter-transformers.git@longt5
3. gpu set up with pytorch
4. the guts to handle possibly-exhausted disk space or ram if tight

## Steps

1. make funny noises
2. in the git repository of your choosing, run hist.bash. generates *.file and *.commit files in parent directory
3. squirm around confusedly
4. while waiting for hist.bash to boil, run hist2json.py . generates test.json from *.file and *.commit files.
5. forget what you are doing by accident, then return.
6. while hist2json.py simmers, run example_run_summarization.bash . optionally modify to taste: up MAX_IN_LEN, MAX_OUT_LEN, BATCH_SIZE, or EPOCHS if you have more than 2GB gpu ram, and more speed and time.
7. blend way too much and let sit for an hour or however long you feel like
8. have fun trying to figure out how to use the model adapter trained on git history. serves 2-3 confused software developers and/or machine learning hobbyists.

## Explanation

PLEASE EXPLAIN

