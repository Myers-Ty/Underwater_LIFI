# Function to send an Ethernet frame
import contextlib
import logging
import os
import socket
import sys
from typing import Iterator
from scapy.all import Ether
from scapy.all import raw
from math import ceil
import socket

PACKET_SIZE = 44 # Define a constant for packet size

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

def recv_eth_frame(eth_type: int, eth_if: str = '') -> str:
    with configure_eth_if(eth_type, eth_if) as so:
        so.settimeout(50)
        try:
            eth_frame = Ether(so.recv(128))
            if eth_frame.type == eth_type:
                logging.info('Received %d bytes from %s', len(eth_frame), eth_frame.src)
                logging.info('Received msg: "%s"', eth_frame.load.decode())
        except Exception as e:
            raise e
    return str(eth_frame.load.decode().rstrip('\x00'))

def stringify_length(length: int) -> str:
    length_str = ''
    while(length > 0):
        digit = length % 256
        length //= 256
        char_digit = chr(digit)
        length_str = char_digit + length_str
    return length_str

def send_large_data(data: str, eth_type: int, dest_mac: str, eth_if: str = '', ) -> None:
    total_length = len(data)
    offset = 0
    chunk_size = PACKET_SIZE
    number_of_packets = ceil(total_length / chunk_size)
    length_str = ''

    length_str = stringify_length(total_length)

    send_eth_frame(f"LONGPACKET[" + length_str + "]", eth_type, dest_mac, eth_if)

    chunk_size = PACKET_SIZE - len(length_str)

    i = 0

    while offset < total_length:
        packet_num = stringify_length(i)
        while(len(packet_num) < len(length_str)):
            packet_num = chr(0) + packet_num
        chunk = packet_num + data[offset:offset + chunk_size] 
        send_eth_frame(chunk, eth_type, dest_mac, eth_if)
        offset += chunk_size
        i += 1

def send_file(file_path: str, eth_type: int, dest_mac: str, eth_if: str = '') -> None:
    with open(file_path, 'r') as f:
        data = f.read()
        send_large_data(data, eth_type, dest_mac, eth_if)