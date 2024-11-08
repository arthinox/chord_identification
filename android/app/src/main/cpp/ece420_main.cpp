//
// Created by daran on 1/12/2017 to be used in ECE420 Sp17 for the first time.
// Modified by dwang49 on 1/1/2018 to adapt to Android 7.0 and Shield Tablet updates.
//

#include "ece420_main.h"
#include "ece420_lib.h"
#include "kiss_fft/kiss_fft.h"

// JNI Function
extern "C" {
JNIEXPORT float JNICALL
Java_com_ece420_lab4_MainActivity_getFreqUpdate(JNIEnv *env, jclass);
}

// Student Variables
#define F_S 48000
#define FRAME_SIZE 1024
#define VOICED_THRESHOLD 123456789  // Find your own threshold
float lastFreqDetected = -1;

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

    lastFreqDetected = -1;

//  Check voiced / unvoiced
//  If unvoiced, freq = -1
    float E = 0;

    for (float x : bufferIn) {
        if (x < 0) {
            x = -x;
        }
        x = x * x;
        E = E + x;
    }

//    Everything else should go inside this block
    if (E > 100000000) {
        kiss_fft_cpx dataIn[FRAME_SIZE] = {};
        kiss_fft_cpx fftOut[FRAME_SIZE] = {};
        kiss_fft_cpx autocor0[FRAME_SIZE] = {};
//        Fill dataIn
        for (int i = 0; i < FRAME_SIZE; i++) {
            dataIn[i].r = bufferIn[i];
            dataIn[i].i = 0;
        }

        kiss_fft_cfg cfg1 = kiss_fft_alloc(FRAME_SIZE, 0, nullptr, nullptr);
//        Compute FFT
        kiss_fft(cfg1, dataIn, fftOut);
//        Compute autocorrelation
        for (auto & i : fftOut) {
            i.r = (i.r * i.r) + (i.i * i.i);
            i.i = 0;
        }
        kiss_fft_cfg cfg2 = kiss_fft_alloc(FRAME_SIZE, 1, nullptr, nullptr);
        kiss_fft(cfg2, fftOut, autocor0);
        float autocor[FRAME_SIZE / 2] = {};
        for (int i = 0; i < FRAME_SIZE / 2; i++) {
            autocor[i] = autocor0[i].r;
        }
        int max_l = findMaxArrayIdx(autocor, 25, (FRAME_SIZE / 2) - 10);
        lastFreqDetected = F_S / float(max_l);
        free(cfg1);
        free(cfg2);
    }



//  Compute FFT, then autocorrelation

//  Diregarding indices close to zero, find index of max value between
//  l = 0 and l = N / 2

    // ********************* END YOUR CODE HERE ************************* //
    gettimeofday(&end, NULL);
    LOGD("Time delay: %ld us",  ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
}

JNIEXPORT float JNICALL
Java_com_ece420_lab4_MainActivity_getFreqUpdate(JNIEnv *env, jclass) {
    return lastFreqDetected;
}