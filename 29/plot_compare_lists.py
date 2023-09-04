#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np

# traditional = [0.019966, 0.095837, 0.180244, 0.195340]
# approximate = [0.021546, 0.108778, 0.191885, 0.248192]
standard_list = [0.001116,0.000255,0.000244,0.000283,0.000322,0.000918,0.000402,0.000838,0.000961,0.001052,0.001209,0.001692,0.001135,0.001706,0.001668,0.001845]
hand_over_hand_list = [0.000598,0.000269,0.000291,0.000299,0.000702,0.000247,0.000471,0.000376,0.000653,0.000467,0.000762,0.000575,0.001030,0.001271,0.000995,0.001185]
a = np.arange(1, len(standard_list)+1)
s = 2 ** a
img_file = 'list_compare.png'
# plt.plot(a, traditional, marker='x')
plt.plot(a, standard_list)
plt.plot(a, hand_over_hand_list)
plt.margins(0)
# plt.xticks(a, s)
plt.xlabel('thread num')
plt.ylabel('Time (seconds)')
plt.legend(['standard', 'hand_over_hand'])
plt.savefig(img_file, dpi=300)
plt.show()
