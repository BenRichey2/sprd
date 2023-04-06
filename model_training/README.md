# Training SPRD Speech Recognition Model w/ Docker
### by Ben Richey

In this tutorial, you'll learn how to build a custom Docker image that is
prepared to train the speech recognition model used in the SPRD device. In
addition, you'll learn how to use a container based on the image to train
a model, monitor the training process using a Tensorboard on your computer's
browser, export the model to your computer, and alter the training parameters.

## Installing Docker

Docker is an open source platform that allows you to build, run, deploy, and
ship applications. It works on every major OS, and I chose to use it to
ensure that all users have the same experience training the model regardless of
which platform they are on. You can learn more and install Docker [here](https://docs.docker.com/get-docker/)

## Building the Docker Image

Assuming you have Docker installed on your machine and it is running, you can
build the image by opening a terminal in the `model_training` directory and
running the following command:

```
docker build -t model_trainer .
```

This should work on Windows, Mac, and Linux. Assuming it completes successfully,
you should see this image available in your Docker engine.

## Training the Model

The `train_model.sh` script contains the command and parameters used to train
the model. If you'd like to play with different parameter configurations, feel
free to alter the parameters at the top of the script.

To train the model, you'll need to create a new docker container based on the
image built in the steps above with the following command:

```
docker run -it -p 6006:6006 model_trainer
```

This will create a container and allow you to interact with it in the terminal
you ran that command in.

First, we will start up Tensorboard, which is a utility by TensorFlow that
allows you to monitor the training process. You can do that with the following
commands in the terminal you just created to the running docker container:

```
cd
tensorboard --logdir ./logs --bind_all
```

This will allow you to monitor the training process through your web browser.
Go ahead and open your web browser and type `localhost:6006` into the address
bar. You should see a Tensorboard webpage that has some message about no data
available yet. It should look like this:

![Empty Tensorboard](./images/empty_tensorboard.png)

Now, you're ready to start training. You'll need to open a new terminal and
connect to your running docker container. You can find the name of your running
docker container you just created with the following command:

```
docker container ls
```

This command will list all of your docker containers on your system. If there's
more than one, it's up to you to figure out which one is the one you just
created. Some context clues are if it's actively running, how long it's been
running, and the image it was made from. You can connect a new terminal to the
running container using the following command, where you replace
"conatiner_name" with the name of your container you found in the above command:

```
docker exec -it container_name bash
```

Now, using the terminal you just opened, you can start trainig with the
following commands:

```
cd
./train_model.sh
```

The training process should begin in that terminal. This will likely take a
while, particularly because the computation is being done by the processor, so
depending on how many cores you have on your CPU and how much RAM you have,
it'll likely take over an hour.

After the training set has downloaded, training will begin, and you can monitor
it by refreshing the Tensorboard on your browser. In the Tensorboard web page,
hit refresh, and then click the "Scalars" tab to view graphs of the accuracy and
loss. It should look something like this:

![Full Tensorboard](./images/TensorBoardDuringTraining.png)

## Saving the model

Once the training is complete, you can save the model to .tflite and C byte
array formats using the following command in the terminal where you executed
the train command:

```
./save_model.sh
```

The C byte array format of the trained model will be stored in `/root/model.cc`
of the docker container. The quantized and floating point versions of the
.tflite format will be stored in the `/root/models/` directory of the docker
container. If you'd like to use these trained model files, you can copy a file
`filename` located in path `file_path` from the docker container
`container_name` to the location `path` on your host machine using the following
command:

```
docker cp container_name:file_path/filename path
```

## Altering training parameters

You can alter training parameters such as the number of epochs, learning rate,
words to be trained on, and more by opening the `train_model.sh` script and
changing the parameter values at the top of the script. Once you've done this,
be sure to save the changes and rebuild the docker image before beginning a new
training session for your alterations to take effect. Certain parameters are
common to the `train_model.sh`, `save_model.sh`, and `tf_to_tflite.py`
programs, so if you make changes, be sure to update all three.

## Adding Your Own Training Data

Recording your own data using the SPRD device and adding it to the training set
can improve the performance of the SPRD device in terms of accuracy. Follow
these instructions to do so.

1. Follow the steps in the README file of the `audio_recorder` folder to collect
your own audio recordings. You should record as many samples as you have time
for, and try to say the keyword once every second.

2. Next, you'll need to separate the samples into 1 second clips. Since
`audio_recorder` records 5 second clips, you'll need to manually separate these
into 1 second samples. I recommend downloading
[Audacity](https://www.audacityteam.org/). It's a free and open-source audio
editing software. Use that to separate your 5 second clip into distinct clips
that are 1 second or less where the keyword is spoken only 1 time per clip. Save
each of these 1 second or less clips as their own waveform file.

3. Due to the way the audio samples are pre-processed during the training phase,
you must name each sample according to the following convention:
`<speaker_name>_nohash_<num>.wav`, where `<speaker_name>` is the name of the
person who was recorded (an alias, or nickname will do, it just needs to be
uniquely identifying) and `<num>` indicates the sample number. For example, if
Bobby spoke a keyword 5 times, there would be 5 clips that are 1 second or less
with the names `Bobby_nohash_0.wav`, `Bobby_nohash_1.wav`, ...,
`Bobby_nohash_4.wav`.

4. For each keyword that you record data, you should create a folder in the
`model_training/custom_data` folder where the name is the keyword. For example,
if you recorded examples of people saying the word "up", you would create a
folder called `up`. Then, you should store all examples of "up" in the newly
created folder. Continuing the Bobby example from step 3, your folder structure
should look like this:
```
    sprd/
        model_training/
            custom_data/
                up/
                    - Bobby_nohash_0.wav
                    - Bobby_nohash_1.wav
                    - Bobby_nohash_2.wav
                    - Bobby_nohash_3.wav
                    - Bobby_nohash_4.wav
```

5. Once you have all of your training data prepared and stored in the
`custom_data` folder, you will need to rebuild the docker image as described in
the `Building the Docker Image` section of this document. Then, you can follow
the steps in this document to train a model, and your data will be added to the
training set! Hopefully this newly trained model will perform better on your
device.

