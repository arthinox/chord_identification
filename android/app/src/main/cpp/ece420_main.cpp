//
// Created by daran on 1/12/2017 to be used in ECE420 Sp17 for the first time.
// Modified by dwang49 on 1/1/2018 to adapt to Android 7.0 and Shield Tablet updates.
//

#include "ece420_main.h"
#include "ece420_lib.h"
#include "kiss_fft/kiss_fft.h"
#include <string>
#include <queue>

using namespace std;

#include <cmath>

// JNI Function
extern "C" {
JNIEXPORT jstring JNICALL
Java_com_ece420_lab4_MainActivity_getChordUpdate(JNIEnv *env, jclass);
}

// Student Variables
#define F_S 48000
#define FRAME_SIZE 2048
#define VOICED_THRESHOLD 123456789  // Find your own threshold
#define BUFFER_SIZE (2 * FRAME_SIZE)
float bufferIn[BUFFER_SIZE] = {};
string output;
queue<int> last_prefixes;
queue<int> last_chord_types;
vector<string> Note_Names = {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B"};

void ece420ProcessFrame(sample_buf *dataBuf) {
    // Keep in mind, we only have 20ms to process each buffer!
    struct timeval start;
    struct timeval end;
    gettimeofday(&start, NULL);

    // Shift old data back to make room for the new data
    for (int i = 0; i < FRAME_SIZE; i++) {
        bufferIn[i] = bufferIn[i + FRAME_SIZE - 1];
    }

    // Put in new data
    // Data is encoded in signed PCM-16, little-endian, mono
    for (int i = 0; i < FRAME_SIZE; i++) {
        bufferIn[i + FRAME_SIZE - 1] = ((uint16_t) dataBuf->buf_[2 * i]) | (((uint16_t) dataBuf->buf_[2 * i + 1]) << 8);
        bufferIn[i + FRAME_SIZE - 1] = (float) bufferIn[i + FRAME_SIZE - 1];
    }

    int N = BUFFER_SIZE;

    kiss_fft_cpx dataIn[BUFFER_SIZE] = {};
    kiss_fft_cpx fftOut[BUFFER_SIZE] = {};

    // Fill dataIn
    for (int i = 0; i < N; i++) {
        dataIn[i].r = bufferIn[i];
        dataIn[i].i = 0;
    }

    kiss_fft_cfg cfg1 = kiss_fft_alloc(N, 0, nullptr, nullptr);

    // Compute FFT
    kiss_fft(cfg1, dataIn, fftOut);

    free(cfg1);

    // ************** DENOISING **************** //

    int half_N = N / 2;

    vector<float> denoised_magnitude(half_N, 0);
    float maxMagnitude = 0;

    // Determine maximum magnitude of FFT
    for (int i = 0; i < half_N; i++) {
        denoised_magnitude[i] = sqrt((fftOut[i].r * fftOut[i].r) + (fftOut[i].i * fftOut[i].i));
        if (denoised_magnitude[i] > maxMagnitude) {
            maxMagnitude = denoised_magnitude[i];
        }
    }

    // Manual threshold function
    float threshold = maxMagnitude * 0.1;
    for (int i = 0; i < half_N; i++) {
        if (denoised_magnitude[i] < threshold) {
            denoised_magnitude[i] = 0;
            // Zero out
            fftOut[i].r = 0;
            fftOut[i].i = 0;
        }
    }

    vector<float> hps(half_N, 0.0);

    for (int i=0; i < half_N; i++) {
        float prod = denoised_magnitude[i];
        if ((2*i < half_N) && (denoised_magnitude[2*i] > 0)) {
            prod *= denoised_magnitude[2*i];
        }
        if ((4*i < half_N) && (denoised_magnitude[4*i] > 0)) {
            prod *= denoised_magnitude[4*i];
        }
        if ((8*i < half_N) && (denoised_magnitude[8*i] > 0)) {
            prod *= denoised_magnitude[8*i];
        }
        hps[i] = sqrt(sqrt(prod));
    }

    // Calculate IPCP
    float f_ref = 130.81278265; // Define middle C (C3) as our reference freq.
    vector<float> ipcp(12, 0.0);

    for (int k = 1; k < half_N; k++) {
        // Find p(k)
        int p_k = int(round(12 * log2((float(k) / float(half_N)) * (F_S / f_ref)))) % 12;
        // Add squared amplitude to corresponding bin
        ipcp[p_k] += hps[k] * hps[k];
    }

    // Square root, then normalize
    for (auto& value : ipcp) {
        value = sqrt(value);
    }
    float Z = *max_element(ipcp.begin(), ipcp.end());

    for (auto& value : ipcp) {
        value /= Z;
    }

    //Circular shifting and template matching
    int NUM_CHORD_TYPES = 6;

    vector<vector<float>> templates = {  // 7 Chord Types
            {0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //minor
            {0.333, -0.333, -0.333, -0.333, 0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //major
            {0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //sus2
            // {0.333, -0.333, -0.333, -0.333, -0.333, 0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //sus4
            {0.25,  -0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25}, //7
            {0.25,  -0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  -0.25,  0.25}, //maj7
            {0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25} //min7
    };

    vector<string> prefix = {"C" , "C#/Db" , "D" , "D#/Eb" , "E" , "F" , "F#/Gb" , "G" , "G#/Ab" , "A" , "A#/Bb" , "B" , "n/a"};
    vector<string> chord_types = {" minor" , " major" , " sus2", " 7" , " maj7" , " min7" , " " };

    int max_info[2] = {12, NUM_CHORD_TYPES}; //defaults to 'n/a'
    float max_score = 0.0;

    for (int i = 0; i < 12; i++){
        for (int j = 0; j < NUM_CHORD_TYPES; j++){
            float temp = 0.0;
            for (int x = 0; x < 12; x++){
                temp += ipcp[(x+i)%12] * templates[j][x];
            }
            if (temp > max_score){
                max_info[0] = i;
                max_info[1] = j;
                max_score = temp;
            }
        }
    }

    // Output mode of last 6 measurements for stability
    if ((int)last_prefixes.size() >= 6) {
        last_prefixes.pop();
        last_chord_types.pop();
        last_prefixes.push(max_info[0]);
        last_chord_types.push(max_info[1]);
    }
    else {
        last_prefixes.push(max_info[0]);
        last_chord_types.push(max_info[1]);
    }

    // Find mode of each queue
    vector<int> hist1(13);
    vector<int> hist2(NUM_CHORD_TYPES + 1);
    queue<int> temp1(last_prefixes);
    queue<int> temp2(last_chord_types);
    for (int i=0; i < (int)last_prefixes.size(); i++) {
        int val1 = temp1.front();
        int val2 = temp2.front();
        hist1[val1] += 1;
        hist2[val2] += 1;
        temp1.pop();
        temp2.pop();
    }

    int mode_prefix = distance(hist1.begin(),max_element(hist1.begin(), hist1.end()));
    int mode_chord_type = distance(hist2.begin(),max_element(hist2.begin(), hist2.end()));

    output = prefix[mode_prefix] + chord_types[mode_chord_type];

    if (chord_types[mode_chord_type] == " sus2") {
        output = output + " or " + prefix[(mode_prefix + 7) % 12] + " sus4";
    }

//    char buf[100];
//    strcpy(buf, "Output: ");
//    strcat(buf, output.c_str());
//    LOGD("%s", buf);
    // ********************* END YOUR CODE HERE ************************* //
    gettimeofday(&end, NULL);
    LOGD("Time delay: %ld us",  ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
}

JNIEXPORT jstring JNICALL
Java_com_ece420_lab4_MainActivity_getChordUpdate(JNIEnv *env, jclass) {
    jstring tempString = env->NewStringUTF(output.c_str());

    return tempString;
}