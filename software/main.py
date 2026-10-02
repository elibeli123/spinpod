'''
The main program launches the UI, allowing controll of the rest of the software stack. 
'''

import tkinter as tk
from ui import ExperimentUI
from controller import ExperimentController
from Serial_Logger import SerialExperiment

def main():
    root = tk.Tk()
    serial_exp = SerialExperiment()
    controller = ExperimentController(serial_exp)
    ExperimentUI(root, controller)
    root.mainloop()

if __name__ == "__main__":
    main()


