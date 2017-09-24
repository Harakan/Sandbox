
import numpy

SEED=10000000

for i in range(0,SEED):
    print (numpy.random.choice(6,6, replace=False)+1)
