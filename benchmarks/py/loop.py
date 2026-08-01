import time

n = 10000000 + (int(time.time()) % 1000)
acc = 0
for i in range(n):
    acc += 1
print("loop-n=", n)
print("loop-acc=", acc)
