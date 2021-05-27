import xlrd
import matplotlib.pyplot as plt

workbook = xlrd.open_workbook('./chartData.xls')
booksheet = workbook.sheet_by_index(0)

peerN = booksheet.col_values(0)[1:]
C_S1 = booksheet.col_values(1)[1:]
C_S2 = booksheet.col_values(2)[1:]
P2P1 = booksheet.col_values(3)[1:]
P2P2 = booksheet.col_values(4)[1:]

C_S = []
P2P = []
for i in range(len(C_S1)):
    C_S.append((C_S1[i]+C_S2[i])/2)
    P2P.append((P2P1[i]+P2P2[i])/2)

l1=plt.plot(peerN,C_S,'r--',label='C&S')
l2=plt.plot(peerN,P2P,'b--',label='P2P')
plt.title('C/S and P2P contrast')
plt.xlabel('peer num')
plt.ylabel('time(ms)')
plt.legend()
plt.show()