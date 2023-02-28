#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

#include "AudioFile.h"

#define NUM_CHANNELS 1
#define SAMPLING_FREQ 12000
#define BIT_DEPTH 16
#define LENGTH_IN_S 1
#define GAIN 1000


int main()
{
    // Get csv file name from user
    std::cout << "Enter csv filename to convert to wav: ";
    if (std::cin.peek() == '\n') std::cin.ignore();
    std::string filename;
    getline(std::cin, filename);

    // Open given file and load data into vector
    std::vector<float> raw_data;
    std::fstream csv_file(filename, std::ios::in);
    if (csv_file.is_open()) {
        std::string line, datum;
        getline(csv_file, line);
        std::stringstream str(line);
        while(getline(str, datum, ',')) {
            raw_data.push_back(std::stof(datum));
        }
        csv_file.close();
    } else {
        std::cout << "Error: Could not open file." << std::endl;
        return -1;
    }

    // Check we have at least 1s worth of audio data
    if (raw_data.size() < SAMPLING_FREQ) {
        std::cout << "Error: Not enough data in CSV file." << std::endl;
        return -1;
    }

    // Create audio file and load audio data into it
    AudioFile<float> a_file;
    a_file.setNumChannels(NUM_CHANNELS);
    a_file.setNumSamplesPerChannel(LENGTH_IN_S * SAMPLING_FREQ);
    a_file.setSampleRate(SAMPLING_FREQ);
    a_file.setBitDepth(BIT_DEPTH);
    for (int channel = 0; channel < a_file.getNumChannels(); channel++) {
        for (int i = 0; i < a_file.getNumSamplesPerChannel(); i++) {
            a_file.samples[channel][i] = GAIN * raw_data[i];
        }
    }
    // Prompt user to enter where they want to save the file
    std::cout << "Enter file name to store .wav file to: ";
    if (std::cin.peek() == '\n') std::cin.ignore();
    getline(std::cin, filename);
    a_file.save(filename);

    return 0;
}