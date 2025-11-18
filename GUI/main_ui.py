from PySide6.QtWidgets import QApplication, QWidget, QGridLayout
from UI.incoming_data_widget import IncomingDataWidget

from UI.outgoing_data_widget import OutgoingDataWidget
from UI.metric_widget import MetricWidget
# from outgoing_metric_widget import OutgoingDataMetricWidget
# from incoming_metric_widget import IncomingDataMetricWidget
from Packets.send_logic import send_eth_frame
import sys

app = QApplication(sys.argv)

window = QWidget()

grid_layout = QGridLayout()

outgoing_data_widget = OutgoingDataWidget()

grid_layout.addWidget(outgoing_data_widget, 0, 0)
grid_layout.addWidget(IncomingDataWidget(), 0, 1)
grid_layout.addWidget(MetricWidget(), 1, 0)
grid_layout.addWidget(MetricWidget(), 1, 1)

# listen to the outgoing data widget's send signal
def handle_send_message(message: str):
    # get destination MAC address
    dest_mac = outgoing_data_widget.get_mac()


    print(f"Handle sending message: {message}")
    # Here you would add the logic to actually send the message via your communication protocol
    send_eth_frame(message, eth_type=0x0800, dest_mac=dest_mac)  # Example usage
    
outgoing_data_widget.send_signal.connect(handle_send_message)

window.setLayout(grid_layout)



window.show()
app.exec()
