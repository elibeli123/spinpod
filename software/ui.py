'''
This program implements a graphical user interface (GUI) for configuring and running the multiplexed LED-sensor experiment. The interface allows the user to select the serial port, specify the output CSV file, choose which sensor systems to run, and define LED brightness settings either globally or for individual modules. When the experiment starts, the program builds a configuration object containing these parameters and passes it to a controller that manages communication with the hardware. The interface also displays system status and logs messages during execution to monitor the experiment in real time.

'''

import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext

LED_LABELS = ["695nm", "465nm", "850nm", "640nm", "569nm"]


class ExperimentUI:
    def __init__(self, root, controller):
        self.root = root
        self.controller = controller

        self.root.title("Multiplexing Experiment Controller")

        self.port_var = tk.StringVar(value="/dev/tty.usbserial-10")
        self.file_var = tk.StringVar()

        self.run_mode_var = tk.StringVar(value="list")

        self.system_vars = [tk.BooleanVar(value=False) for _ in range(8)]

        self.use_global_brightness_var = tk.BooleanVar(value=False)
        self.global_brightness_vars = [tk.StringVar(value="100") for _ in range(5)]

        # NEW: per-module override checkboxes
        self.module_override_vars = [tk.BooleanVar(value=False) for _ in range(8)]

        self.module_brightness_vars = []
        for _ in range(8):
            row = [tk.StringVar(value="100") for _ in range(5)]
            self.module_brightness_vars.append(row)

        self.build_ui()

    def build_ui(self):
        pad = {"padx": 6, "pady": 4}

        tk.Label(self.root, text="Port").grid(row=0, column=0, sticky="w", **pad)
        tk.Entry(self.root, textvariable=self.port_var, width=30).grid(
            row=0, column=1, sticky="ew", **pad
        )

        tk.Label(self.root, text="CSV File").grid(row=1, column=0, sticky="w", **pad)
        tk.Entry(self.root, textvariable=self.file_var, width=50).grid(
            row=1, column=1, columnspan=3, sticky="ew", **pad
        )
        tk.Button(self.root, text="Browse", command=self.browse_file).grid(
            row=1, column=4, **pad
        )

        run_frame = tk.LabelFrame(self.root, text="Run Mode")
        run_frame.grid(row=2, column=0, columnspan=7, sticky="ew", padx=8, pady=6)

        tk.Radiobutton(
            run_frame, text="LIST=selected", variable=self.run_mode_var, value="list"
        ).grid(row=0, column=0, sticky="w", **pad)
        tk.Radiobutton(
            run_frame, text="EXCEPT=selected", variable=self.run_mode_var, value="except"
        ).grid(row=0, column=1, sticky="w", **pad)
        tk.Radiobutton(
            run_frame, text="LIST=ALL", variable=self.run_mode_var, value="all"
        ).grid(row=0, column=2, sticky="w", **pad)

        systems_frame = tk.LabelFrame(self.root, text="Systems")
        systems_frame.grid(row=3, column=0, columnspan=7, sticky="ew", padx=8, pady=6)

        for i in range(8):
            tk.Checkbutton(
                systems_frame, text=str(i), variable=self.system_vars[i]
            ).grid(row=0, column=i, **pad)

        global_frame = tk.LabelFrame(self.root, text="Global Brightness (optional B=...)")
        global_frame.grid(row=4, column=0, columnspan=7, sticky="ew", padx=8, pady=6)

        tk.Checkbutton(
            global_frame,
            text="Use global brightness",
            variable=self.use_global_brightness_var,
        ).grid(row=0, column=0, sticky="w", columnspan=2, **pad)

        for j in range(5):
            tk.Label(global_frame, text=LED_LABELS[j]).grid(
                row=1, column=2 * j, sticky="e", **pad
            )
            tk.Entry(
                global_frame, textvariable=self.global_brightness_vars[j], width=8
            ).grid(row=1, column=2 * j + 1, **pad)

        module_frame = tk.LabelFrame(self.root, text="Per-Module Brightness Overrides (optional BMx=...)")
        module_frame.grid(row=5, column=0, columnspan=7, sticky="ew", padx=8, pady=6)

        tk.Label(module_frame, text="Use").grid(row=0, column=0, padx=4, pady=4)
        tk.Label(module_frame, text="Module").grid(row=0, column=1, padx=4, pady=4)
        for j in range(5):
            tk.Label(module_frame, text=LED_LABELS[j]).grid(
                row=0, column=j + 2, padx=4, pady=4
            )

        for i in range(8):
            tk.Checkbutton(module_frame, variable=self.module_override_vars[i]).grid(
                row=i + 1, column=0, padx=4, pady=2
            )
            tk.Label(module_frame, text=str(i)).grid(row=i + 1, column=1, padx=4, pady=2)
            for j in range(5):
                tk.Entry(
                    module_frame,
                    textvariable=self.module_brightness_vars[i][j],
                    width=8,
                ).grid(row=i + 1, column=j + 2, padx=4, pady=2)

        tk.Button(self.root, text="Start", command=self.on_start).grid(row=6, column=0, **pad)
        tk.Button(self.root, text="Stop", command=self.on_stop).grid(row=6, column=1, sticky="w", **pad)

        self.status_label = tk.Label(self.root, text="Idle", anchor="w")
        self.status_label.grid(row=6, column=2, columnspan=5, sticky="w", **pad)

        self.log_box = scrolledtext.ScrolledText(
            self.root, width=110, height=20, state="disabled"
        )
        self.log_box.grid(row=7, column=0, columnspan=7, sticky="nsew", padx=8, pady=8)

        self.root.grid_columnconfigure(1, weight=1)
        self.root.grid_rowconfigure(7, weight=1)

    def browse_file(self):
        filename = filedialog.asksaveasfilename(
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if filename:
            self.file_var.set(filename)

    def log(self, msg):
        def _append():
            self.log_box.configure(state="normal")
            self.log_box.insert(tk.END, msg + "\n")
            self.log_box.see(tk.END)
            self.log_box.configure(state="disabled")
        self.root.after(0, _append)

    def set_running_state(self, running: bool):
        def _set():
            self.status_label.config(text="Running" if running else "Idle")
        self.root.after(0, _set)

    def _selected_systems(self):
        return [i for i, var in enumerate(self.system_vars) if var.get()]

    def _parse_brightness_row(self, row_vars, label):
        vals = []
        for v in row_vars:
            s = v.get().strip()
            try:
                n = int(s)
            except ValueError:
                raise ValueError(f"{label}: brightness entries must be integers.")
            if n < 0 or n > 255:
                raise ValueError(f"{label}: brightness entries must be 0..255.")
            vals.append(n)
        return vals

    def build_config(self):
        run_mode = self.run_mode_var.get()
        systems = self._selected_systems()

        global_brightness = None
        if self.use_global_brightness_var.get():
            global_brightness = self._parse_brightness_row(
                self.global_brightness_vars,
                "Global brightness",
            )

        # ONLY include module overrides if their checkbox is checked
        module_brightness = {}
        for i in range(8):
            if self.module_override_vars[i].get():
                module_brightness[i] = self._parse_brightness_row(
                    self.module_brightness_vars[i],
                    f"Module {i}",
                )

        return {
            "port": self.port_var.get().strip(),
            "filepath": self.file_var.get().strip(),
            "run_mode": run_mode,
            "systems": systems,
            "global_brightness": global_brightness,
            "module_brightness": module_brightness,
        }

    def on_start(self):
        try:
            config = self.build_config()
            self.controller.start(config, self.log, self.set_running_state)
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def on_stop(self):
        self.controller.stop()
