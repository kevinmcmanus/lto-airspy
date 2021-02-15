import matplotlib
matplotlib.use('TkAgg')

import numpy as np
import matplotlib.pyplot as plt
import scipy.io.wavfile as wv

xx = np.arange(10)
yy = xx**2 + 3

fig = plt.figure(figsize=(6,4))
ax = fig.add_subplot()

ax.plot(xx,yy)

plt.show()
