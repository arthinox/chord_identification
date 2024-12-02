//
// Created by daran on 1/12/2017 to be used in ECE420 Sp17 for the first time.
// Modified by dwang49 on 1/1/2018 to adapt to Android 7.0 and Shield Tablet updates.
//

#include "ece420_main.h"
#include "ece420_lib.h"
#include "kiss_fft/kiss_fft.h"
#include <string>

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

//Circular shifting and template matching
   int NUM_CHORD_TYPES = 7;
   
           //             [0,        1,      2,      3,    4,       5,       6,     7,     8,      9,      10,      11])    
           //                                2       m3    M3       4              5              6        b7      M7
   float templates[NUM_CHORD_TYPES][12] = {
                          {0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //minor
                          {0.333, -0.333, -0.333, -0.333, 0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //major
                          {0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //sus2
                          {0.333, -0.333, -0.333, -0.333, -0.333, 0.333, -0.333, 0.333, -0.333, -0.333, -0.333, -0.333}, //sus4
                          {0.25,  -0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25}, //7
                          {0.25,  -0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  -0.25,  0.25}, //maj7
                          {0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  -0.25,  0.25,  -0.25,  -0.25,  -0.25,  0.25} //min7
                         };
   
   string prefix[13] = {" C" , " C#/Db" , " D" , " D#/Eb" , " E" , " F" , " F#/Gb" , " G" , " G#/Ab" , " A" , " A#/Bb" , " B" , " n/a"}; 
   string chord_types[NUM_CHORD_TYPES + 1] = {"  minor" , "  major" , " sus2" , " sus4" , " 7" , " maj7" , " min7" , "  n/a" };
   
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

    //Here are the outputs
    std::cout<<prefix[max_info[0]]; 
    std::cout<<chord_types[max_info[1]];

        

        

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
