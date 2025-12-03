import time
import os
import pwd
from PySide6.QtWidgets import QFileDialog, QWidget, QVBoxLayout, QPushButton, QLineEdit, QTextBrowser, QComboBox
from PySide6.QtGui import QIntValidator
from PySide6.QtCore import QTimer

from .progress_bar import ProgressBar90



class IncomingDataWidget(QWidget):
    def __init__(self):
        super().__init__()

        self.files = dict()
        self.box_layout = QVBoxLayout()
        self.setLayout(self.box_layout)

        # self.progressBar = ProgressBar90()


        self.receieved_select = QComboBox()
        self.endButton = QPushButton("Save File")
        self.endButton.clicked.connect(lambda : self.select_path_to_save(self.receieved_select.currentText()))
        self.box_layout.addWidget(self.receieved_select)
        self.box_layout.addWidget(self.endButton)

        # self.number = QLineEdit()
        # self.number.setText('5')
        # self.number.setValidator(QIntValidator(0, 100, self.number))

        # This will be changed once we have proper hooks
        self.startButton = QPushButton("Receieve File")
        self.startButton.clicked.connect(self.run_progress_bar)

        # self.box_layout.addWidget(self.number)
        self.box_layout.addWidget(self.startButton)
        # self.box_layout.addWidget(self.progressBar)

                # This will be changed once we have proper hooks
        # self.endButton = QPushButton("End Progress Bar")
        # self.endButton.clicked.connect(lambda : self.progressBar.end_progress_bar())
        self.box_layout.addWidget(self.endButton)
        self.log = QTextBrowser()
        self.box_layout.addWidget(self.log) 

    def add_log(self, message: str):
        self.log.append(time.strftime("[%H:%M:%S]") + " " + message)    

    def run_progress_bar(self):
        self.progressBar.run_progress_bar(int(self.number.text()))


    def add_file(self, title: str, content: str):
        self.files[title] = content.rstrip(b'\x00')
        self.receieved_select.addItem(title)
        self.add_log(f"File '{title}' received with size {len(content)} bytes.")

    def select_path_to_save(self, title: str):
        # Placeholder for file dialog to select path
        path, _ = QFileDialog.getSaveFileName(self, "Save File", title)
        if path:
            self.save_file(title, path)

    def save_file(self, title: str, path: str):
        if title in self.files:
            with open(path, 'wb') as f:
                f.write(self.files[title])
            user_name = os.getenv('SUDO_USER') or os.getenv('USER')
            user_info = pwd.getpwnam(user_name)
            os.chown(path, user_info.pw_uid, user_info.pw_gid)
            self.receieved_select.removeItem(self.receieved_select.currentIndex())
            self.files.pop(title)
            self.add_log(f"File '{title}' saved to '{path}'.")
        else:
            self.add_log(f"File '{title}' not found.")


