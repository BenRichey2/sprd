FROM ubuntu:20.04
RUN apt update
RUN apt install -y \
    git \
    python3 \
    python3-pip \
    curl \
    xxd
RUN python3 -m pip install tensorflow numpy
RUN mkdir ~/train ~/dataset ~/logs ~/models
RUN git clone -q --depth 1 https://github.com/tensorflow/tensorflow ./root/tensorflow
COPY ./train_model.sh /root/
COPY ./tf_to_tflite.py /root/
COPY ./save_model.sh /root/