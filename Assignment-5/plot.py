import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# max_memory = 250 * 1024 * 1024

# fd = pd.read_csv('gc_data.csv')
# gc_data = fd.loc[:, "gc"].to_numpy()
# non_gc_data = fd.loc[:, "non_gc"]

gc_data = []
non_gc_data = []
with open('2_gc.txt') as file:
    gc_data = [line.rstrip() for line in file]

with open('2_non_gc.txt') as file:
    non_gc_data = [line.rstrip() for line in file]

gc_data = np.array(gc_data).astype(np.int)
non_gc_data = np.array(non_gc_data).astype(np.int)

# gcl = len(gc_data)
# ngcl = len(non_gc_data)
# i = 0
# j = 0

# new_non_gc_data = []
# new_non_gc_data.append(gc_data)
# while i < gcl:



# if len(non_gc_data) < len(gc_data):
#     for i in range(len(gc_data) - len(non_gc_data)):
#         # non_gc_data.append(non_gc_data[-1])
#         non_gc_data = np.append(non_gc_data, non_gc_data[-1])

# print(gc_data)
# print(non_gc_data)

gc_avg = np.mean(gc_data)
gc_std_dev = np.std(gc_data)
gc_max = np.max(gc_data)

non_gc_avg = np.mean(non_gc_data)
non_gc_std_dev = np.std(non_gc_data)
non_gc_max = np.max(non_gc_data)

print('GC: mean = {}, std_dev = {}, max = {}'.format(gc_avg, gc_std_dev, gc_max))
print('Non-GC: mean = {}, std_dev = {}, max = {}'.format(non_gc_avg,
                                                         non_gc_std_dev, non_gc_max))

plt.subplots_adjust(left=0.15)
plt.plot(gc_data, 'ro-', label='With GC', linestyle='-')
plt.xlabel('Time')
plt.ylabel('Memory Usage')
plt.plot(non_gc_data, 'bx-', label='Without GC', linestyle=':')
# plt.axhline(y=max_memory, color='g', linestyle='-', label='Max Memory')
plt.legend()
plt.savefig('2_gc_plot.png')
