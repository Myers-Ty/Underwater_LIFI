
import socket
from PySide6.QtWidgets import QApplication, QWidget, QGridLayout
from PySide6.QtCore import QThread, Signal, QTimer, QObject
from UI.incoming_data_widget import IncomingDataWidget
from queue import Queue
import datetime

from UI.outgoing_data_widget import OutgoingDataWidget
from UI.metric_widget import MetricWidget
# from outgoing_metric_widget import OutgoingDataMetricWidget
# from incoming_metric_widget import IncomingDataMetricWidget
from Packets.send_logic import construct_socket, requeue_dropped, send_large_data, send_loop, queue_eth_frame, recv_eth_frame, intify_length, send_file, PACKET_SIZE, set_dropped, get_receiving_large, handle_large_data_packet, start_receive_large, clear_Packet_queue
import sys
ETH_TYPE_2 = 0x2221
ETH_TYPE_3 = 0x2223
RECIEVE_QUEUE = Queue()

app = QApplication(sys.argv)

window = QWidget()

grid_layout = QGridLayout()
grid_layout.setContentsMargins(2, 0, 2, 2)  # left, top, right, bottom
grid_layout.setHorizontalSpacing(2)
grid_layout.setVerticalSpacing(0)

outgoing_data_widget = OutgoingDataWidget()
incoming_data_widget = IncomingDataWidget()
metric_widget = MetricWidget()

grid_layout.addWidget(outgoing_data_widget, 0, 0)
grid_layout.addWidget(incoming_data_widget, 0, 1)
grid_layout.addWidget(metric_widget, 1, 0, 2, 2)
grid_layout.setRowStretch(0, 1)  # Outgoing/incoming widgets row
grid_layout.setRowStretch(1, 3)  # Metric widget row (increase this value for more space)ut.addWidget(MetricWidget(), 1, 1)




class MetricSignalEmitter(QObject):
    update_dropped_signal = Signal(int, int)
    update_duration_signal = Signal(float, int)
    log_signal = Signal(str)
    


metric_signal_emitter = MetricSignalEmitter()

def log_to_widget(msg):
    incoming_data_widget.add_log(msg)

metric_signal_emitter.log_signal.connect(log_to_widget)

def update_metric_dropped(lost_count, kept_count):
    metric_widget.update_dropped(lost_count, kept_count)

def update_metric_throughput(duration, size_bytes):
    metric_widget.update_throughput(duration, size_bytes)

metric_signal_emitter.update_dropped_signal.connect(update_metric_dropped)
metric_signal_emitter.update_duration_signal.connect(update_metric_throughput)

class MessageSenderThread(QThread):
    def __init__(self, message=None, is_large=False, file_path=None):
        super().__init__()
        self.message = message
        self.is_large = is_large
        self.file_path = file_path

    def run(self):
        if self.file_path:
            send_file(self.file_path)
        elif self.is_large:
            send_large_data(self.message.encode(), title='MESSAGE')
        else:
            queue_eth_frame(self.message.encode())


# Keep references to sender threads to prevent garbage collection
sender_threads = []

def handle_send_file(file_path):
    thread = MessageSenderThread(file_path=file_path)
    sender_threads.append(thread)
    thread.finished.connect(lambda: sender_threads.remove(thread))
    thread.start()

def handle_send_message(message: str):
    metric_signal_emitter.log_signal.emit(f"Sending message: {message}")
    if len(message) >= PACKET_SIZE:
        metric_signal_emitter.log_signal.emit(f"Message length {len(message)} exceeds PACKET_SIZE {PACKET_SIZE}, sending as large data.")
        thread = MessageSenderThread(message=message, is_large=True)
    else:
        thread = MessageSenderThread(message=message, is_large=False)
    sender_threads.append(thread)
    thread.finished.connect(lambda: sender_threads.remove(thread))
    thread.start()

# listen to the outgoing data widget's send signal
def handle_send_message(message: str):
    # get destination MAC address

    #log the message being sent
    metric_signal_emitter.log_signal.emit(f"Sending message: {message}")
    # Here you would add the logic to actually send the message via your communication protocol
    if(len(message) >= PACKET_SIZE):
        metric_signal_emitter.log_signal.emit(f"Message length {len(message)} exceeds PACKET_SIZE {PACKET_SIZE}, sending as large data.")    
        send_large_data(message.encode(), title='MESSAGE')
    else:
        queue_eth_frame(message.encode())  # Example usage
    
outgoing_data_widget.send_signal.connect(handle_send_message)
outgoing_data_widget.send_file_signal.connect(handle_send_file)
outgoing_data_widget.clear_signal.connect(lambda : clear_Packet_queue())

window.setLayout(grid_layout)

def handle_receieve_message(so: socket.socket) -> bytes:
    return recv_eth_frame(so)


def receiver_queue_loop():
    so = construct_socket(ETH_TYPE_3)
    try:
        while True:
            try:
                message = handle_receieve_message(so)
                RECIEVE_QUEUE.put(message)
            except Exception as e:
                pass
    finally:
        so.close()




def receiver_event_loop():
    print("Starting receiver event loop")
    while True:
        try:
            message = RECIEVE_QUEUE.get()
            if message is None:
                continue
            print(f"Received message (hex): {message.hex()}")
            if(message.__contains__(b"PNUM[")):
                start_index = message.index(b"[") + 1
                end_index = message.index(b"]")
                length_bytes = message[start_index:end_index]
                length = intify_length(length_bytes)
                timestamp = message[end_index+1:end_index+4]
                minute = timestamp[0]
                second = timestamp[1]
                millisecond = timestamp[2]  # since you divided microseconds by 10000, this is centiseconds
                packet_time = datetime.time(minute=minute, second=second, microsecond=millisecond * 10000)
                metric_signal_emitter.log_signal.emit(f"Preparing to receive {length} packets")
                start_receive_large(length, packet_time)
                continue
            if message.startswith(b'DROPPED['):
                start_index = message.index(b"[") + 1
                dropped_bytes = message[start_index:PACKET_SIZE]
                print(f"dropped message is {message}")
                requeue_dropped(dropped_bytes)
                metric_signal_emitter.log_signal.emit(f"PACKET_BUFFER_FULL")
                set_dropped(True)
                continue
            if message.startswith(b'LOST['):
                message = message.rstrip(b'\x00')
                message = str(message)
                start_index = message.index("[") + 1
                end_index = message.index("]")
                lost_count_bytes = message[start_index:end_index]
                lost_count = int(lost_count_bytes)
                start_index = message.index("KEPT[") + 5
                end_index = len(message) - 2
                kept_count_bytes = message[start_index:end_index]
                kept_count = int(kept_count_bytes)
                metric_signal_emitter.log_signal.emit(f"PACKET_LOST broadcast: {lost_count} lost | {kept_count} kept")
                metric_signal_emitter.update_dropped_signal.emit(lost_count, kept_count)
                continue
            if get_receiving_large() > 0:
                # process large packet
                data = handle_large_data_packet(message)
                if data is None:
                    continue
                title, message, duration = data
                
                metric_signal_emitter.log_signal.emit(f"Received large message with title: {title}")
                metric_signal_emitter.update_duration_signal.emit(duration, len(message) * 8)

                if(title == 'MESSAGE'):
                    metric_signal_emitter.log_signal.emit(message.decode(errors='ignore').rstrip('\x00'))
                    continue
                incoming_data_widget.add_file(title, message)
                continue
                
            metric_signal_emitter.log_signal.emit("Received Message: " + message.decode(errors='ignore').rstrip('\x00'))

        except Exception as e:
            continue

receiver_thread = QThread()
receiver_thread.run = receiver_queue_loop
receiver_thread.start()

receiver_event_thread = QThread()
receiver_event_thread.run = receiver_event_loop
receiver_event_thread.start()

sender_thread = QThread()
sender_thread.run = lambda: send_loop(ETH_TYPE_2, 'ff:ff:ff:ff:ff:ff')
sender_thread.start()

window.setWindowTitle("LIFI GUI")
window.show()
app.exec()
