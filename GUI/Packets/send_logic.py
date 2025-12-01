# Function to send an Ethernet frame
import contextlib
import logging
import os
import socket
import sys
from typing import Iterator
from scapy.all import Ether
from scapy.all import raw
import socket


@contextlib.contextmanager
def configure_eth_if(eth_type: int, target_if: str = '') -> Iterator[socket.socket]:
    if target_if == '':
        # try to determine which interface to use
        netifs = os.listdir('/sys/class/net/')
        # order matters - ETH NIC with the highest number is connected to DUT on CI runner
        netifs.sort(reverse=True)
        logging.info('detected interfaces: %s', str(netifs))
        for netif in netifs:
            if netif.find('eth') == 0 or netif.find('enx') == 0 or netif.find('enp') == 0 or netif.find('eno') == 0:
                target_if = netif
                break
        if target_if == '':
            raise Exception('no network interface found')
    logging.info('Use %s for testing', target_if)

    so = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(eth_type))
    so.bind((target_if, 0))

    try:
        yield so
    finally:
        so.close()

def send_eth_frame(message: str, eth_type: int, dest_mac: str, eth_if: str = '') -> None:
    with configure_eth_if(eth_type, eth_if) as so:
        so.settimeout(10)
        eth_frame = Ether(dst=dest_mac, src=so.getsockname()[4], type=eth_type) / raw(message.encode())
        try:
            so.send(raw(eth_frame))
            logging.info('Sent %d bytes to %s', len(eth_frame), dest_mac)
            logging.info('Sent msg: "%s"', message)

            # eth_frame_repl = Ether(so.recv(128))
            # if eth_frame_repl.type == eth_type:
            #     logging.info('Received %d bytes echoed from %s', len(eth_frame_repl), eth_frame_repl.src)
            #     logging.info('Echoed msg: "%s"', eth_frame_repl.load.decode())
        except Exception as e:
            raise e
    # return echoed message and remove possible null characters which might have been appended since
    # minimal size of Ethernet frame to be transmitted physical layer is 60B (not including CRC)
    return "done"