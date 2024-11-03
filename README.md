# Musical Chord Identification
Final Project for ECE 420.

## Abstract
Musical chord identification, or the process of correctly labeling three or more notes played
simultaneously, is a well-researched topic with significant implications for musical transcription
\and analysis. However, factors such as instrument-dependent overtones and chord-obscuring
noise have rendered this task difficult. To gain insight into how it is achieved at a basic level,
we will try to implement a real-time chord identification algorithm for Android mobile devices.
Our algorithm will be able to distinguish between and correctly identify an isolated set of
simultaneous tones being played. It will then select the correct name of the chord (e.g. C major,
F minor) based on the notes identified. As a baseline, we will extend the concepts used in Lab 4
to attempt this task. Afterward, we will implement methods such as pitch class profiles and
template matching to improve the accuracy and versatility of our system. An additional component
of our project will be the sound synthesis with which we will test our algorithm. We will
initially test our algorithm on the simplest case—chords comprised of three sine-wave tones, and
then evaluate our accuracy under those conditions. Afterward, we will experiment with tones that
feature greater harmonic content (e.g. triangle waves, sawtooth waves) and compare our results.
