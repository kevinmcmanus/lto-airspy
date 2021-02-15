import matplotlib
matplotlib.use('TkAgg')

import numpy as np
import matplotlib.pyplot as plt
import scipy.io.wavfile as wv


def get_pwr_from_wav(wvpath):
    rate, obs = wv.read(wvpath)
    obs_f = obs.astype(float)
    pwr = np.sqrt(obs_f[:,0]**2 + obs_f[:,1]**2)
    return pwr

tests = ['test1', 'test2', 'test3']

old_test = {}
for t in tests:
    old_test[t] = get_pwr_from_wav('data.old/{}.wav'.format(t))

new_test = {}
for t in tests:
    new_test[t] = get_pwr_from_wav('data.new/{}.wav'.format(t))




s = slice(7,10000, None)
fig = plt.figure(figsize=(24,27))
ax = fig.subplots(3,2)
for i,t in enumerate(tests):
    ax[i,0].plot(old_test[t][s], color='blue', label='Test Case: {}, Old Code'.format(i))
    ax[i,1].plot(new_test[t][s], color='red',  label='Test Case: {}, New Code'.format(i))
    for a in ax[i]:
        a.legend(loc='upper right')

plt.show()
