# Function to send an Ethernet frame
import contextlib
import logging
import os
import socket
import sys
from typing import Iterator
from scapy.all import Ether
from scapy.all import raw

def configure_eth_if(eth_type: int, eth_if: str):
    # Placeholder for actual socket configuration
    import socket
    so = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(eth_type))
    if eth_if:
        so.bind((eth_if, 0))
    return so

def send_eth_frame(message: str, eth_type: int, dest_mac: str, eth_if: str = '') -> None:
    with configure_eth_if(eth_type, eth_if) as so:
        eth_frame = Ether(dst=dest_mac, src=so.getsockname()[4], type=eth_type) / raw(message.encode())
        so.send(raw(eth_frame))
        logging.info('Sent %d bytes to %s', len(eth_frame), dest_mac)
        logging.info('Sent msg: "%s"', message)