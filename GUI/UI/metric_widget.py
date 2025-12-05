from PySide6.QtWidgets import QRadioButton, QWidget, QGridLayout, QSizePolicy
from pyqtgraph import PlotWidget, plot

class DataRadioButton(QRadioButton):
    def __init__(self, label:str, units:str):
        super().__init__()

        self.label = label
        self.units = units
        # self.update_dropped(self.data)
        self.data = 0.0

        self.update_label()

    def set_data(self, data: float) -> None:
        self.data = data
        self.update_label()
        
    def update_label(self) -> None:
        if self.data is None:
            self.setText(f"{self.label} N/A {self.units}")
        else:
            self.setText(f"{self.label} {self.data:.3f} {self.units}")
                # Assuming data is a float


        

class MetricWidget(QWidget):
    def __init__(self):
        super().__init__()
        self.grid_layout = QGridLayout()
        self.setLayout(self.grid_layout)

        self.dropped_data : list[tuple[int, int]] = []
        self.throughput_data : list[tuple[float, int]] = []

        self.throughput_mode = True
        self.drop_mode = False


        self.throughput_radio = DataRadioButton("ThroughRate:", "b/s")
        self.packet_radio = DataRadioButton("PacketLoss", "%")

        self.grid_layout.addWidget(self.throughput_radio, 0, 0)
        self.grid_layout.addWidget(self.packet_radio, 1, 0)
        self.plot = PlotWidget()
        self.plot.enableAutoRange(axis='y', enable=True)
        self.plot.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.grid_layout.addWidget(self.plot, 0, 1, 2, 1)
        self.grid_layout.setRowStretch(0, 3)  # Plot row
        self.grid_layout.setRowStretch(1, 0)  # Radio button row
        self.grid_layout.setColumnStretch(0, 0)  # Radio button column
        self.grid_layout.setColumnStretch(1, 3)  # Plot column
        self.packet_radio.clicked.connect(lambda: self.set_packet_plot())
        self.throughput_radio.clicked.connect(lambda: self.set_throughput_plot())


    def update_dropped(self, dropped_count: int, kept_count: int) -> None:

        self.dropped_data.append((dropped_count, kept_count))
        if len(self.dropped_data) > 100:
            self.dropped_data.pop(0)

        avg_dropped = sum([d[0] for d in self.dropped_data]) / (10 * len(self.dropped_data)) * 100
        self.packet_radio.set_data(avg_dropped)

        if self.drop_mode:
            self.set_packet_plot()

    def update_throughput(self, duration: float, size_bytes: int) -> None:
        if(len(self.throughput_data) >= 100):
            self.throughput_data.pop(0)
        self.throughput_data.append((duration, size_bytes))
        total_throughput = sum([size for (dur, size) in self.throughput_data]) / sum([dur for (dur, size) in self.throughput_data])
        self.throughput_radio.set_data(total_throughput)
        if self.throughput_mode:
            self.set_throughput_plot()

    def set_packet_plot(self) -> None:
        self.plot.clear()
        if len(self.dropped_data) > 1:
            x = [i for i in range(len(self.dropped_data))]
            y = [d[0] / (d[0] + d[1]) * 100 for d in self.dropped_data]
            self.plot.plot(x, y, pen='r', name='Packet Loss %')
        else:
            self.packet_radio.set_data(None)
        self.drop_mode = True
        self.throughput_mode = False

    def set_throughput_plot(self) -> None:
        self.plot.clear()
        if len(self.throughput_data) > 0:
            x = [i for i in range(len(self.throughput_data))]
            y = [size / dur for (dur, size) in self.throughput_data]
            self.plot.plot(x, y, pen='g', name='Throughput b/s')
        else:
            self.throughput_radio.set_data(None)
        self.throughput_mode = True
        self.drop_mode = False