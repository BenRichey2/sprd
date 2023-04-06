FROM ubuntu:20.04
RUN apt update
RUN apt install -y \
    git \
    python3 \
    python3-pip \
    curl \
    xxd
RUN python3 -m pip install tensorflow numpy
RUN mkdir ~/train ~/dataset ~/logs ~/models ~/custom_data
RUN git clone -q --depth 1 https://github.com/BenRichey2/tensorflow.git ./root/tensorflow
RUN cd /root/tensorflow && git checkout -b sprd && git pull origin sprd
COPY ./train_model.sh /root/
COPY ./tf_to_tflite.py /root/
COPY ./save_model.sh /root/
COPY ./custom_data /root/custom_data