import numpy as np
from scipy.io.wavfile import read, write
from numpy.fft import fft, ifft
import math

# Sampling rate
Fs = 44100

# Given a denoised amplitude spectrum
N = 1024
spec_old = np.zeros(int(N/2))
spec_new = np.zeros(int(N/2))
size = spec_old.shape[0]

# Multiply downsampled copies (M=4) to obtain HPS
for k in range(size):
    prod = spec_old[k]
    if (2*k < size):
        prod *= spec_old[2*k]
    if (4*k < size):
        prod *= spec_old[4*k]
    if (8*k < size):
        prod *= spec_old[8*k]
    spec_new[k] = np.sqrt(np.sqrt(prod))
    
# Calculate IPCP
# Define middle C (C3) as our reference freq.
f_ref = 130.81278265

ipcp = np.zeros(12)

for k in range(size):
    # Find p(k)
    p_k = np.round(12*math.log2((k/float(N))*(Fs/float(f_ref)))) % 12
    # Add squared amplitude to corresponding bin
    ipcp[p_k] += spec_new[k]**2

for i in range(12):
    # Square root, then divide by z
    ipcp = np.sqrt(ipcp)
    Z = np.max(ipcp)
    ipcp = ipcp / float(Z)

notes = ind = np.argpartition(ipcp, -3)[-3:]