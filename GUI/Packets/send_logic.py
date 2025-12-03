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
import time

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

def send_eth_frame(message: bytes, eth_type: int, dest_mac: str, eth_if: str = '') -> None:
    with configure_eth_if(eth_type, eth_if) as so:
        so.settimeout(10)
        eth_frame = Ether(dst=dest_mac, src=so.getsockname()[4], type=eth_type) / raw(message)
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

def recv_eth_frame(eth_type: int, eth_if: str = '') -> bytes:
    with configure_eth_if(eth_type, eth_if) as so:
        so.settimeout(50)
        try:
            eth_frame = Ether(so.recv(128))
            if eth_frame.type == eth_type:
                logging.info('Received %d bytes from %s', len(eth_frame), eth_frame.src)
                logging.info('Received msg: "%s"', eth_frame.load.decode(errors='ignore'))
                # print(f"Received msg: {eth_frame.load}")
        except Exception as e:
            raise e
    return eth_frame.load

def intify_length(length_str: bytes) -> int:
    length = 0
    for char in length_str:
        length = (length << 8) + char
    return length

def byte_length(length: int) -> bytes:
    length_bytes = b''
    while length > 0:
        length_bytes = bytes([length & 0xFF]) + length_bytes
        length = length >> 8
    return length_bytes

def find_number_of_packets(length: int) -> int:
    meta_data_length = 1
    while(True):
        chunk_size = PACKET_SIZE - meta_data_length
        number_of_packets = ceil(length / chunk_size) + 1  # +1 for the naming packet
        if len(byte_length(number_of_packets)) <= meta_data_length:
            # print(f"Meta data length: {meta_data_length}")
            return number_of_packets
        meta_data_length += 1



def send_large_data(data: bytes, eth_type: int, dest_mac: str, eth_if: str = '', title: str = 'msg.txt') -> None:
    total_length = len(data)
    # print(f"Total data length: {total_length} bytes")
    offset = 0
    number_of_packets = find_number_of_packets(total_length)
    length_bytes = byte_length(number_of_packets)
    # print(f"Meta data length: {len(length_bytes)} bytes")
    

    send_eth_frame(b"LONGPACKET[" + length_bytes + b"]", eth_type, dest_mac, eth_if)
    chunk_size = PACKET_SIZE - len(length_bytes)

    i = 1
    time.sleep(0.1) 
    packet_num_bytes = byte_length(i)
    while(len(packet_num_bytes) < len(length_bytes)):
            packet_num_bytes = b'\x00' + packet_num_bytes
    send_eth_frame(packet_num_bytes + b"TITLE[" + title.encode() + b"]", eth_type, dest_mac, eth_if)
    i += 1

    print(f"Sending {number_of_packets} packets...")
    while offset < total_length:
        time.sleep(0.1)  # slight delay to avoid overwhelming the receiver
        packet_num_bytes = byte_length(i)
        while(len(packet_num_bytes) < len(length_bytes)):
            packet_num_bytes = b'\x00' + packet_num_bytes
        chunk = packet_num_bytes + data[offset:offset + chunk_size]
        # print(f"[SEND] Packet {i}: num_bytes={packet_num_bytes.hex()} chunk_len={len(data[offset:offset + chunk_size])} offset={offset}")
        # print(f"[SEND] Chunk data (hex): {data[offset:offset + chunk_size].hex()}")
        send_eth_frame(chunk, eth_type, dest_mac, eth_if)
        offset += chunk_size
        # print("Sent packet %d/%d" % (i, number_of_packets))
        i += 1

def send_file(file_path: str, eth_type: int, dest_mac: str, eth_if: str = '') -> None:
    with open(file_path, 'rb') as f:
        data = f.read()
        send_large_data(data, eth_type, dest_mac, eth_if, title=file_path.split('/')[-1])

def recv_large_data(eth_type: int, eth_if: str = '', length: int = 0) -> tuple[str, bytes]:
    data = b''
    received_packets: list[tuple[int, bytes]] = [] 
    number_of_packets = length
    meta_data_length = len(byte_length(length))
    while len(received_packets) < number_of_packets:
        chunk = recv_eth_frame(eth_type, eth_if)
        packet_num = intify_length(chunk[0:meta_data_length])
        content = chunk[meta_data_length:PACKET_SIZE]
        # print(f"[RECV] Packet {packet_num}: content_len={len(content)} content (hex): {content.hex()}")
        if(packet_num, content) not in received_packets and packet_num <= number_of_packets and packet_num > 0:
            received_packets.append((packet_num, content))
            # print("Received packet %d/%d" % (packet_num, number_of_packets))

    received_packets.sort(key=lambda x: x[0])
    name_packet = received_packets[0]
    # Extract title from name packet, making sure to remove trailing 0s
    title = name_packet[1].rstrip(b'\x00')[len(b"TITLE[") + meta_data_length - 1: -1].decode(errors='ignore')   
    received_packets = received_packets[1:]  # Remove the title packet

    for packet in received_packets:
        data += packet[1]
    # print(f"[RECV] Total reconstructed data length: {len(data)} bytes")
    return title, data