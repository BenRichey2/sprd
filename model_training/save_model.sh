WANTED_WORDS="up,down,left,right"
WINDOW_STRIDE=20
PREPROCESS="micro"
MODEL_ARCHITECTURE="tiny_conv"
TRAIN_DIR="/root/train/"
MODEL_DIR="/root/models/"
START_CHECKPOINT="15000"
SAVE_FORMAT="saved_model"

python3 tensorflow/tensorflow/examples/speech_commands/freeze.py --wanted_words=$WANTED_WORDS --window_stride_ms=$WINDOW_STRIDE --preprocess=$PREPROCESS --model_architecture=$MODEL_ARCHITECTURE --start_checkpoint=$TRAIN_DIR$MODEL_ARCHITECTURE".ckpt-"$START_CHECKPOINT --save_format=$SAVE_FORMAT --output_file=$MODEL_DIR$SAVE_FORMAT
python3 tf_to_tflite.py
xxd -i /root/models/model.tflite > model.cc