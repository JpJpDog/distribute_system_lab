#!/usr/bin/python

"""
Simple example of setting network and CPU parameters
"""

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import OVSBridge
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel, info
from mininet.cli import CLI

from sys import argv

# It would be nice if we didn't have to do this:
# pylint: disable=arguments-differ


class Homework1Topo(Topo):
    def build(self):
        hostN = 5
        switch1 = self.addSwitch('s1', stp=True)
        for index in range(hostN):
            host = self.addHost('h'+str(index+1), cpu=.25)
            self.addLink(host, switch1, loss=0, bw=1, use_htb=True)
        myScript = "cmd.sh"
        file = open(myScript, 'w')
        file.write('h1 ./server &\n')
        for i in range(hostN-1):
            index = i+2
            file.write('h'+str(index)+' ./peer '+str(index)+' &\n')
        file.close()


def Test():
    topo = Homework1Topo()
    net = Mininet(topo=topo,
                  host=CPULimitedHost, link=TCLink,
                  autoStaticArp=False)
    net.start()
    CLI(net, script="cmd.sh")
    CLI(net)
    net.stop()


if __name__ == '__main__':
    setLogLevel('info')
    # Prevent test_simpleperf from failing due to packet loss
    Test()
