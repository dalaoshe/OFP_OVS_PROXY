from mininet.topo import *
from mininet.topolib import *
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.node import Controller, RemoteController,OVSSwitch
from optparse import OptionParser
from mininet.log import setLogLevel
from mininet.cli import CLI
import sys
import time

class MyTreeTopo( Topo ):
    "Topology for a tree network with a given depth and fanout."

    def build( self, depth=1, fanout=2 ):
        # Numbering:  h1..N, s1..M
        self.hostNum = 1
        self.switchNum = 1
        # Build topology
        #self.addTree( depth, fanout )

        s = list()
        h =list()
        for i in range(1, 3):
            s.append(self.addSwitch('s%s' % str(i)))

        self.addLink(s[0], s[1], bw=1000, max_queue_size=100000)
        self.addLink(s[0], s[1], bw=1000, max_queue_size=100000)

        for i in range(1, 3):
            host = self.addHost('h%s' % str(i))
            h.append(host)
            self.addLink(host, s[0], bw=1000, max_queue_size=100000)

        for i in range(1, 3):
            host = self.addHost('h%s' % str(i + 2))
            h.append(host)
            self.addLink(host, s[1], bw=1000, max_queue_size=100000)
        #self.add
        #self.addLink()

    def addTree( self, depth, fanout ):
        """Add a subtree starting with node n.
           returns: last node added"""
        isSwitch = depth > 0
        if isSwitch:
            node = self.addSwitch( 's%s' % self.switchNum )
            self.switchNum += 1
            for _ in range( fanout ):
                child = self.addTree( depth - 1, fanout )
                self.addLink( node, child, bw=10000, max_queue_size=10000)
        else:
            node = self.addHost( 'h%s' % self.hostNum )
            self.hostNum += 1
        return node

def simpleTest(ip, port, rate):
    topo = MyTreeTopo(depth = 3, fanout = 2)
    net = Mininet(topo=topo, switch=OVSSwitch,
        controller=lambda name: RemoteController(name, ip=ip, port=int(port)),
        autoSetMacs=True, link=TCLink)
    net.start()
    # print 'sleeping...'
    # time.sleep(30)
    # print 'attack beginning...'
    hosts = net.hosts
    # cmd = './attackOf.sh ' + rate + ' &'
    # hosts[1].cmd(cmd)
    # pid2 = int(hosts[1].cmd('echo $!'))
    # hosts[3].cmd(cmd)
    # pid4 = int(hosts[3].cmd('echo $!'))
    # hosts[7].cmd(cmd)
    # pid8 = int(hosts[7].cmd('echo $!'))
    # hosts[0].cmd('./attackOf.sh 30 &')
    # pid1 = int(hosts[0].cmd('echo $!'))
    # hosts[2].cmd('./attackOf.sh 30 &')cd M
    # pid3 = int(hosts[2].cmd('echo $!'))
    CLI(net)
    # hosts[0].cmd('kill -9 ', pid1)
    # hosts[1].cmd('kill -9 ', pid2)
    # hosts[2].cmd('kill -9 ', pid3)
    # hosts[3].cmd('kill -9 ', pid4)
    # hosts[7].cmd('kill -9 ', pid8)
    #net.stop()

if __name__ == '__main__':
    setLogLevel('info')
simpleTest(sys.argv[1], sys.argv[2], 0)