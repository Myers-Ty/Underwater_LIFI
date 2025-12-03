
from PySide6.QtWidgets import QApplication, QWidget, QGridLayout
from PySide6.QtCore import QThread, Signal
from UI.incoming_data_widget import IncomingDataWidget

from UI.outgoing_data_widget import OutgoingDataWidget
from UI.metric_widget import MetricWidget
# from outgoing_metric_widget import OutgoingDataMetricWidget
# from incoming_metric_widget import IncomingDataMetricWidget
from Packets.send_logic import send_eth_frame, recv_eth_frame, intify_length, recv_large_data, send_file
import sys
ETH_TYPE_2 = 0x2221
ETH_TYPE_3 = 0x2223

app = QApplication(sys.argv)

window = QWidget()

grid_layout = QGridLayout()

outgoing_data_widget = OutgoingDataWidget()
incoming_data_widget = IncomingDataWidget()

grid_layout.addWidget(outgoing_data_widget, 0, 0)
grid_layout.addWidget(incoming_data_widget, 0, 1)
grid_layout.addWidget(MetricWidget(), 1, 0)
grid_layout.addWidget(MetricWidget(), 1, 1)

# listen to the outgoing data widget's send signal
def handle_send_message(message: str):
    # get destination MAC address
    dest_mac = outgoing_data_widget.get_mac()


    print(f"Handle sending message: {message}")
    # Here you would add the logic to actually send the message via your communication protocol
    send_eth_frame(message.encode(), ETH_TYPE_2, 'ff:ff:ff:ff:ff:ff')  # Example usage
    
outgoing_data_widget.send_signal.connect(handle_send_message)
outgoing_data_widget.send_file_signal.connect(lambda file_path: send_file(file_path, ETH_TYPE_2, 'ff:ff:ff:ff:ff:ff'))

window.setLayout(grid_layout)

def handle_receieve_message() -> bytes:
    return recv_eth_frame(ETH_TYPE_3)

def receiver_event_loop():
    print("Starting receiver event loop")
    while True:
        try:
            message = handle_receieve_message()
            print(f"Received raw message: {message}")
            if(message.__contains__(b"LONGPACKET[")):
                print("Processing large packet...\n\n\n\n\n\n")
                # process large packet, getting length from between brackets
                start_index = message.index(b"[") + 1
                end_index = message.index(b"]")
                length_bytes = message[start_index:end_index]
                length = intify_length(length_bytes)
                print(f"Expecting {length} packets")
                # receive large data
                title, message = recv_large_data(ETH_TYPE_3, length=length)
                incoming_data_widget.add_log(f"Received large message with title: {title}")
                incoming_data_widget.add_file(title, message)
                continue
            print(f"Received message: {message}")
            incoming_data_widget.add_log(message.decode(errors='ignore'))

        except Exception as e:
            continue

receiver_thread = QThread()
receiver_thread.run = receiver_event_loop
receiver_thread.start()
window.show()
app.exec()
