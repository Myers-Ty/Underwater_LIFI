from PySide6.QtWidgets import QPushButton, QWidget, QGridLayout, QFileDialog, QLineEdit, QSizePolicy
from PySide6.QtCore import Signal
from .progress_bar import ProgressBar90

class OutgoingDataWidget(QWidget):
    send_signal = Signal(str)  # Signal to send text messages - must be class attribute
    send_file_signal = Signal(str)  # Signal to send file paths - must be class attribute
    clear_signal = Signal()  # Signal to clear packet queue - must be class attribute

    def __init__(self):
        super().__init__()

        self.grid_layout = QGridLayout()
        self.setLayout(self.grid_layout)

        self.startButton = QPushButton("Send File")
        self.startButton.clicked.connect(self.select_file)
        
        # self.progressBar = ProgressBar90()

        self.grid_layout.addWidget(self.startButton, 0, 0)
        # self.grid_layout.addWidget(self.progressBar, 1, 0)


                # This will be changed once we have proper hooks
        # self.endButton = QPushButton("End Progress Bar")
        # self.endButton.clicked.connect(lambda : self.progressBar.end_progress_bar())
        # self.grid_layout.addWidget(self.endButton, 2, 0)

        self.send_text_button = QPushButton("Send Text Message")
        self.grid_layout.addWidget(self.send_text_button, 4, 0)
        self.send_text = QLineEdit()
        self.grid_layout.addWidget(self.send_text, 3, 0)
        self.send_text_button.clicked.connect(self.send_text_message)

        # self.mac_input = QLineEdit()
        # self.mac_input.setPlaceholderText("Destination MAC Address (e.g., ff:ff:ff:ff:ff:ff)")
        # self.grid_layout.addWidget(self.mac_input, 5, 0)
        self.reset_button = QPushButton("Reset ESP")
        self.grid_layout.addWidget(self.reset_button, 6, 0)
        self.reset_button.clicked.connect(self.reset_esp)

        self.clear_button = QPushButton("Clear Packet Queue")
        self.grid_layout.addWidget(self.clear_button, 7, 0)
        self.clear_button.clicked.connect(self.clear_packet_queue)
        self.startButton.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.send_text_button.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.reset_button.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.startButton.setStyleSheet("background-color: #2196F3; color: white;")
        self.send_text_button.setStyleSheet("background-color: #2196F3; color: white;")
        self.reset_button.setStyleSheet("background-color: #2196F3; color: white;")

    def clear_packet_queue(self):
        print("Clearing packet queue...")
        self.clear_signal.emit()

    def reset_esp(self):
        print("Resetting ESP...")
        self.send_signal.emit("RESET_ESP|RESET_ESP|RESET_ESP|RESET_ESP")

    def send_text_message(self):
        message = self.send_text.text()
        print(f"Sending message: {message}")
        # tell listeners we want to send this message using QSignal
        self.send_signal.emit(message)
        



    def select_file(self):
        filename, _ = QFileDialog.getOpenFileName(parent=self, caption="Open File", dir=".", filter="All Files (*.*)")
        if filename:
            self.send_file_signal.emit(filename)