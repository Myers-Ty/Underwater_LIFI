
* Create a venv from the requirements.txt file by running
```
  python3 -m venv venv
  source venv/bin/activate

  pip install -r 'requirements.txt'
```

* Then exit the venv and run the test script with the following
```
deactivate
sudo ./venv/bin/python3  TestEth.py
```

* How to use the GUI
...

* If sending packets not via the GUI, you must append a 2 byte CRC to end end. This is calculated by adding each byte of the packet together.