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
#define VOICED_THRESHOLD 123456789.0f
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
    lastFreqDetected = -1;

    // Check voiced / unvoiced
    // If unvoiced, freq = -1
    float E = 0;
    for (float x : bufferIn) {
        if (x < 0) {
            x = -x;
        }
        x = x * x;
        E = E + x;
    }

    if (E > 100000000) {
        // Declare fftOut outside the if block to make it accessible later
        kiss_fft_cpx dataIn[FRAME_SIZE] = {};
        kiss_fft_cpx fftOut[FRAME_SIZE] = {};  // Declare fftOut here
        kiss_fft_cpx autocor0[FRAME_SIZE] = {};

        // Fill dataIn
        for (int i = 0; i < FRAME_SIZE; i++) {
            dataIn[i].r = bufferIn[i];
            dataIn[i].i = 0;
        }

        kiss_fft_cfg cfg1 = kiss_fft_alloc(FRAME_SIZE, 0, nullptr, nullptr);
        // Compute FFT
        kiss_fft(cfg1, dataIn, fftOut);
        // Compute autocorrelation
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

    // ************** DENOISING **************** //
    float denoised_magnitude[FRAME_SIZE / 2] = {};
    float maxMagnitude = 0;

    // Determine maximum magnitude of FFT
    for (int i = 0; i < FRAME_SIZE / 2; i++) {
        denoised_magnitude[i] = sqrt(fftOut[i].r * fftOut[i].r + fftOut[i].i * fftOut[i].i);
        if (denoised_magnitude[i] > maxMagnitude) {
            maxMagnitude = denoised_magnitude[i];
        }
    }

    // Manual threshold function
    float threshold = maxMagnitude * 0.1;
    for (int i = 0; i < FRAME_SIZE / 2; i++) {
        if (denoised_magnitude[i] < threshold) {
            denoised_magnitude[i] = 0;
            // Zero out
            fftOut[i].r = 0;
            fftOut[i].i = 0;
        }
    }

    // ************** PEAK DETECTION **************** //
    float peakFrequencies[4] = {0};  // Array to store detected peaks
    int peakCount = 0;

    for (int i = 1; i < FRAME_SIZE / 2 - 1; i++) {
        if (denoised_magnitude[i] > denoised_magnitude[i - 1] && denoised_magnitude[i] > denoised_magnitude[i + 1] && denoised_magnitude[i] > threshold) {
            // Convert index to frequency
            float freq = i * (F_S / (float)FRAME_SIZE);
            // Store peak frequency
            if (peakCount < 4) {
                peakFrequencies[peakCount] = freq;
                peakCount++;
            }
        }
    }

    // Log or use the detected frequencies as needed
    for (int i = 0; i < peakCount; i++) {
        LOGD("Peak Frequency %d: %f Hz", i + 1, peakFrequencies[i]);
        lastFreqDetected = peakFrequencies[0];
    }

    // ********************* END YOUR CODE HERE ************************* //
    gettimeofday(&end, NULL);
    LOGD("Time delay: %ld us", ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
}

JNIEXPORT float JNICALL
Java_com_ece420_lab4_MainActivity_getFreqUpdate(JNIEnv *env, jclass) {
    return lastFreqDetected;
}
