import time
import math
import os
import tensorflow as tf
import sklearn.model_selection
import random
from eDOCr import keras_ocr

print(tf.config.list_physical_devices('GPU'))

def get_text_generator(alphabet, lowercase=False, max_string_length=None):
    '''
    Generates a sentence.
    Args:
        alphabet: string of characters
        lowercase: convert alphabet to lowercase
        max_string_length: maximum number of characters in the sentence
    Return:
        sentence string
    '''
    while True:
        sentence=random.sample(alphabet,random.randint(5,10))

        if lowercase:
            sentence = sentence.lower()
        sentence = "".join([s for s in sentence if (alphabet is None or s in alphabet)])
        if max_string_length is not None:
            sentence = sentence[:max_string_length]
        yield sentence

def get_train_val_test_split(arr):
    '''
    Splits the dataset in 80% training and 20% validation
    Args: dataset array
    return: arrays for train and validation'''
    train, val = sklearn.model_selection.train_test_split(arr, train_size=0.8, random_state=42)
    return train, val

def generate_n_train(alphabet, backgrounds, fonts, batch_size=12, recognizer_basepath=os.getcwd(),pretrained_model=None):
    '''Starts the training of the recognizer on generated data.
    Args:
    alphabet: string of characters
    backgrounds: list of backgrounds images
    fonts: list of fonts with format *.ttf
    batch_size: batch size for training
    recognizer_basepath: desired path to recognizer
    pretrained_model: path to pretrained weights

    '''
    recognizer_basepath = os.path.join(recognizer_basepath,
    f'recognizer_{time.gmtime(time.time()).tm_hour}'+f'_{time.gmtime(time.time()).tm_min}'+f'_{time.gmtime(time.time()).tm_sec}')

    text_generator = get_text_generator(alphabet)
    background_splits = get_train_val_test_split(backgrounds)
    font_splits = get_train_val_test_split(fonts)

    image_generators = [
        keras_ocr.data_generation.get_image_generator(
            height=640,
            width=640,
            text_generator=text_generator,
            font_groups={
                alphabet: current_fonts
            },
            backgrounds=current_backgrounds,
            font_size=(20, 120),
            margin=10,
            rotationX=(0, 0),
            rotationY=(0, 0),
            rotationZ=(-5, 5)
        )  for current_fonts, current_backgrounds in zip(
            font_splits,
            background_splits
        )
    ]
    recognizer = keras_ocr.recognition.Recognizer(alphabet=alphabet)
    if pretrained_model:
        recognizer.model.load_weights(pretrained_model)
    recognizer.compile()
    for layer in recognizer.backbone.layers:
        layer.trainable = False

    max_length = 10
    recognition_image_generators = [
        keras_ocr.data_generation.convert_image_generator_to_recognizer_input(
            image_generator=image_generator,
            max_string_length=min(recognizer.training_model.input_shape[1][1], max_length),
            target_width=recognizer.model.input_shape[2],
            target_height=recognizer.model.input_shape[1],
            margin=1
        ) for image_generator in image_generators
    ]    

    recognition_train_generator, recognition_val_generator = [
        recognizer.get_batch_generator(
        image_generator=image_generator,
        batch_size=batch_size,
        lowercase=False
        ) for image_generator in recognition_image_generators
    ]

    recognizer.training_model.fit(
        recognition_train_generator,
        epochs=10,
        steps_per_epoch=math.ceil(len(background_splits[0]) / batch_size),
        callbacks=[
            tf.keras.callbacks.EarlyStopping(restore_best_weights=True, patience=5),
            tf.keras.callbacks.CSVLogger(f'{recognizer_basepath}.csv', append=True),
            tf.keras.callbacks.ModelCheckpoint(filepath=f'{recognizer_basepath}.h5',save_best_only=True)
        ],
        validation_data=recognition_val_generator,
        validation_steps=math.ceil(len(background_splits[1]) / batch_size),
        workers=0,
    )

