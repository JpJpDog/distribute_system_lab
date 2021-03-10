import sys

fileName=sys.argv[1]
file = open(fileName, 'r')
line = file.readline()
minStart = sys.maxsize
maxEnd = 0
while line:
    time = int(line[3:])
    if line[0] == 's' and time < minStart:
        minStart = time
    elif line[0] == 'e' and time > maxEnd:
        maxEnd = time
    line = file.readline()
print(minStart)
print(maxEnd)
print(maxEnd-minStart)
file.close()
