from PySide6.QtWidgets import QPushButton, QWidget, QGridLayout, QFileDialog, QLineEdit
from PySide6.QtCore import Signal
from .progress_bar import ProgressBar90

class OutgoingDataWidget(QWidget):
    send_signal = Signal(str)  # Signal to send text messages - must be class attribute
    send_file_signal = Signal(str)  # Signal to send file paths - must be class attribute

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
        self.grid_layout.addWidget(self.send_text_button, 3, 0)
        self.send_text = QLineEdit()
        self.grid_layout.addWidget(self.send_text, 4, 0)
        self.send_text_button.clicked.connect(self.send_text_message)

        # self.mac_input = QLineEdit()
        # self.mac_input.setPlaceholderText("Destination MAC Address (e.g., ff:ff:ff:ff:ff:ff)")
        # self.grid_layout.addWidget(self.mac_input, 5, 0)
        self.reset_button = QPushButton("Reset ESP")
        self.grid_layout.addWidget(self.reset_button, 6, 0)
        self.reset_button.clicked.connect(self.reset_esp)

    def reset_esp(self):
        print("Resetting ESP...")
        self.send_signal.emit("RESET_ESP|RESET_ESP|RESET_ESP|RESET_ESP")

    def send_text_message(self):
        message = self.send_text.text()
        print(f"Sending message: {message}")
        # tell listeners we want to send this message using QSignal
        self.send_signal.emit(message)
        

    def get_mac(self) -> str:
        # For now, return a hardcoded MAC address; this should be replaced with actual input handling
        #assume is ff:ff:ff:ff:ff:ff
        return "ff:ff:ff:ff:ff:ff"
        return self.mac_input.text()


    def select_file(self):
        filename, _ = QFileDialog.getOpenFileName(parent=self, caption="Open File", dir=".", filter="All Files (*.*)")
        if filename:
            self.send_file_signal.emit(filename)