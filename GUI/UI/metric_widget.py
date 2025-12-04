from PySide6.QtWidgets import QRadioButton, QWidget, QGridLayout
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

        self.dropped_data : list[tuple[int, int]] = [(0,0)]
        self.throughput_data : list[float] = []

        self.throughput_mode = True
        self.drop_mode = False


        self.throughput_radio = DataRadioButton("ThroughRate:", "b/s")
        self.packet_radio = DataRadioButton("PacketLoss", "%")

        self.grid_layout.addWidget(self.throughput_radio, 0, 0)
        self.grid_layout.addWidget(self.packet_radio, 1, 0)
        self.plot = PlotWidget()
        self.grid_layout.addWidget(self.plot, 0, 1, 2, 1)

        self.packet_radio.clicked.connect(lambda: self.set_packet_plot())
        self.throughput_radio.clicked.connect(lambda: self.set_throughput_plot())


    def update_dropped(self, dropped_count: int, kept_count: int) -> None:
        self.dropped_data.append((dropped_count + self.dropped_data[-1][0], kept_count + self.dropped_data[-1][1]))
        if len(self.dropped_data) > 100:
            self.dropped_data.pop(0)
        if self.drop_mode:
            self.set_packet_plot()

    def update_throughput(self, throughput: float) -> None:
        if(len(self.throughput_data) >= 100):
            self.throughput_data.pop(0)
        self.throughput_data.append(throughput)
        if self.throughput_mode:
            self.set_throughput_plot()

    def set_packet_plot(self) -> None:
        self.plot.clear()
        if len(self.dropped_data) > 1:
            x = [i for i in range(len(self.dropped_data))]
            y = [d[0] / (d[0] + d[1]) * 100 for d in self.dropped_data]
            self.plot.plot(x, y, pen='r', name='Packet Loss %')
            self.packet_radio.set_data(y[-1])
        else:
            self.packet_radio.set_data(None)
        self.drop_mode = True
        self.throughput_mode = False

    def set_throughput_plot(self) -> None:
        self.plot.clear()
        if len(self.throughput_data) > 0:
            x = [i for i in range(len(self.throughput_data))]
            self.plot.plot(x, self.throughput_data, pen='g', name='Throughput b/s')
            self.throughput_radio.set_data(self.throughput_data[-1])
        else:
            self.throughput_radio.set_data(None)
        self.throughput_mode = True
        self.drop_mode = False