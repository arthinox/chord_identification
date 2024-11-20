//
// Created by daran on 1/12/2017 to be used in ECE420 Sp17 for the first time.
// Modified by dwang49 on 1/1/2018 to adapt to Android 7.0 and Shield Tablet updates.
//

#include "ece420_main.h"
#include "ece420_lib.h"
#include "kiss_fft/kiss_fft.h"

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
std::string output;

std::vector<std::vector<std::string>> Chord_Names = {
        {"C Major", "C#/Db Major", "D Major", "D#/Eb Major", "E Major", "F Major", "F#/Gb Major", "G Major", "G#/Ab Major", "A Major", "A#/Bb Major", "B Major"},
        {"C Minor", "C#/Db Minor", "D Minor", "D#/Eb Minor", "E Minor", "F Minor", "F#/Gb Minor", "G Minor", "G#/Ab Minor", "A Minor", "A#/Bb Minor", "B Minor"}
};

std::vector<std::string> Note_Names = {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B"};

std::vector<int> topThreeIndices(std::vector<float> inVector);

void ece420ProcessFrame(sample_buf *dataBuf) {
    // Keep in mind, we only have 20ms to process each buffer!
    struct timeval start;
    struct timeval end;
    gettimeofday(&start, NULL);

    // Data is encoded in signed PCM-16, little-endian, mono
    float bufferIn[FRAME_SIZE];
    for (int i = 0; i < FRAME_SIZE; i++) {
        int16_t val = ((uint16_t) dataBuf->buf_[2 * i]) | (((uint16_t) dataBuf->buf_[2 * i + 1]) << 8);
        bufferIn[i] = (float) val;
    }

    // ********************** PITCH DETECTION ************************ //
    // In this section, you will be computing the autocorrelation of bufferIn
    // and picking the delay corresponding to the best match. Naively computing the
    // autocorrelation in the time domain is an O(N^2) operation and will not fit
    // in your timing window.
    //
    // First, you will have to detect whether or not a signal is voiced.
    // We will implement a simple voiced/unvoiced detector by thresholding
    // the power of the signal.
    //
    // Next, you will have to compute autocorrelation in its O(N logN) form.
    // Autocorrelation using the frequency domain is given as:
    //
    //  autoc = ifft(fft(x) * conj(fft(x)))
    //
    // where the fft multiplication is element-wise.
    //
    // You will then have to find the index corresponding to the maximum
    // of the autocorrelation. Consider that the signal is a maximum at idx = 0,
    // where there is zero delay and the signal matches perfectly.
    //
    // Finally, write the variable "lastFreqDetected" on completion. If voiced,
    // write your determined frequency. If unvoiced, write -1.
    // ********************* START YOUR CODE HERE *********************** //

    int N = FRAME_SIZE;
    // Declare fftOut outside the if block to make it accessible later
    kiss_fft_cpx dataIn[FRAME_SIZE] = {};
    kiss_fft_cpx fftOut[FRAME_SIZE] = {};  // Declare fftOut here

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

    std::vector<float> denoised_magnitude(half_N, 0);
    float maxMagnitude = 0;

    // Determine maximum magnitude of FFT
    for (int i = 0; i < half_N; i++) {
        denoised_magnitude[i] = std::sqrt((fftOut[i].r * fftOut[i].r) + (fftOut[i].i * fftOut[i].i));
        if (denoised_magnitude[i] > maxMagnitude) {
            maxMagnitude = denoised_magnitude[i];
        }
    }


    // Good up to here!
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

    // If denoised FFT has data type kiss_fft_cpx

    std::vector<float> hps(half_N, 0.0);

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
        hps[i] = std::sqrt(std::sqrt(prod));
    }

    // Calculate IPCP
    float f_ref = 130.81278265; // Define middle C (C3) as our reference freq.
    std::vector<float> ipcp(12, 0.0);

    for (int k = 1; k < half_N; k++) {
        // Find p(k)
        int p_k = int(std::round(12 * std::log2((float(k) / float(half_N)) * (F_S / f_ref)))) % 12;
        // Add squared amplitude to corresponding bin
        ipcp[p_k] += hps[k] * hps[k];
    }



    // Square root, then normalize
    for (auto& value : ipcp) {
        value = std::sqrt(value);
    }
    float Z = *std::max_element(ipcp.begin(), ipcp.end());


    for (auto& value : ipcp) {
        value /= Z;
    }

    // Good up until here!
//    LOGD("C: %f, C#: %f, D: %f, D#: %f, E: %f, F: %f, F#: %f, G: %f", ipcp[0], ipcp[1], ipcp[2], ipcp[3], ipcp[4], ipcp[5], ipcp[6], ipcp[7]);
    std::vector<int> notes(3);
//    std::partial_sort_copy(ipcp.begin(), ipcp.end(), notes.begin(), notes.end(), std::greater<float>());
    // Not good here :(
    notes = topThreeIndices(ipcp);

    std::vector<float> scores(3); // Holds the score each frequency gets if assumed to be the root note

    // Interval Points
    std::vector<float> Interval_Points = {0, 0, 3, 5, 5, 3, 0, 7, 0, 1, 2, 1}; // Holds a score for any given interval. Can be tweaked as wanted
    // Chord Names

    // Try each note as root note, determine a likelihood score for that configuration
    for (int i = 0; i < 3; i++) {
        int d1 = notes[(i + 1) % 3] - notes[i % 3]; // find interval distance 1
        int d2 = notes[(i + 2) % 3] - notes[i % 3]; // find interval distance 2
        if (d1 < 0) { // correct d1 interval for direction
            d1 += 12;
        }
        if (d2 < 0) { // correct d2 interval for direction
            d2 += 12;
        }
        scores[i] = Interval_Points[d1] + Interval_Points[d2];
    }

    // Determine which root note got highest likelihood score
    float max_score = 0;
    int max_score_idx = 0;
    for (int i = 0; i < 3; i++) {
        if (scores[i] > max_score) {
            max_score_idx = i;
            max_score = scores[i];
        }
    }

    // Re-calculate intervals for the winning root note
    int d1 = notes[(max_score_idx + 1) % 3] - notes[max_score_idx % 3]; // find interval distance 1
    int d2 = notes[(max_score_idx + 2) % 3] - notes[max_score_idx % 3]; // find interval distance 2
    if (d1 < 0) { // correct d1 interval for direction
        d1 += 12;
    }
    if (d2 < 0) { // correct d2 interval for direction
        d2 += 12;
    }

    // Check if major or minor, and select chord name
    if (d1 == 3 || d2 == 3) {
        output = Chord_Names[1][notes[max_score_idx]]; // if a minor third is present, select minor chord
    } else {
        output = Chord_Names[0][notes[max_score_idx]]; // else select major chord.
    }

//    char buf[100];
//    strcpy(buf, "Output: ");
//    strcat(buf, output.c_str());
//    LOGD("%s", buf);
        // ********************* END YOUR CODE HERE ************************* //
    gettimeofday(&end, NULL);
//    LOGD("Time delay: %ld us",  ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
}

std::vector<int> topThreeIndices(std::vector<float> inVector) {
    std::vector<int> maxVector(3, 0);
    float first_max = 0;
    float second_max = 0;
    float third_max = 0;

    for (int i = 0; i < inVector.size(); i++) {
        if (first_max < inVector[i]) {
            first_max = inVector[i];
            maxVector[0] = i;
        }
    }

    for (int i = 0; i < inVector.size(); i++) {
        if ((second_max < inVector[i]) && (second_max <= first_max)){
            second_max = inVector[i];
            maxVector[1] = i;
        }
    }

    for (int i = 0; i < inVector.size(); i++) {
        if ((third_max < inVector[i]) && (third_max <= second_max)){
            third_max = inVector[i];
            maxVector[2] = i;
        }
    }

    return maxVector;
}

JNIEXPORT jstring JNICALL
Java_com_ece420_lab4_MainActivity_getChordUpdate(JNIEnv *env, jclass) {
    jstring tempString = env->NewStringUTF(output.c_str());
//    const char *str = env->GetStringUTFChars(tempString, nullptr);
//
//    LOGD("%s", str);
//
//
//// Inform the VM that the native code no longer needs access to str
//    env->ReleaseStringUTFChars(tempString, str);
    return tempString;
}