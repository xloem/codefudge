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

a start of an explanation:

so basically codefudge is some scripts to train an adapter based on
history of a git repository.

hist.bash breaks the git history into *.file files containing commit
message and file content, and *.commit files that contain the file
diff within the commit.    at time of writing, each file changed in
the commit is a separate pair, because i was low on gpu ram. that
system could be improved.

hist2json simply converts those files into data in the format the
huggingface training scripts take. the *.file files are the input, and
the *.commit (diff data) files are the output the model is trained on.

so, the model would possibly learn to produce file changes that match
pairs of commit messages and file contents.

i tried this briefly and my loss got to around 1.5 or so (from 9 or
10) within a few hours on the 16GB gpu i got with google colab pro+
[before the session was terminated and gpu access disabled]

the final trained adapter is only a few megabytes large, despite the
model being several gigabytes.

there's code to run a model forward in forward.py . it expects
a .file on stdin and is supposed to produce a .commit on stdout .

## Notes 

2022-07-23
attempt from first run at https://bafybeib6dbyaesrndgi4awnbx52hvj2rdeyxka7wfulhtnl7icxfhq3jie.ipfs.dweb.link/summarization
doesn't have linebreaks, seems to only handle the first couple lines

a nagging part of me keeps considering the pretraining content of the
model. i'm not sure what t5 models are trained but, i imagine a
generative or masked model would have more general-purpose knowledge
and general language understanding, than something that was simply
trained on translation or summarization type tasks, where word-mapping
without comprehension could get you very far. i should probably look
this up at some point. but anyway i'm thinking of switching to xlnet.

xlnet seems the way to go next, although i'm spending some time stabilising a tokenizer training script
