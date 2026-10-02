'''

This program is the main host-side experiment manager. It validates the user’s configuration, opens the serial connection,
sends the appropriate run-selection and brightness commands to the microcontroller, and then starts the experiment. While the system is running, 
it continuously reads incoming serial data, filters out non-measurement messages, and writes valid spectral data rows to a CSV file. 

It runs this process in a background thread so the GUI remains responsive, and it also handles safe shutdown by sending a stop command, closing the file, 
and disconnecting from the hardware.

'''


import threading
from pathlib import Path
import time
from Serial_Logger import CSV_HEADER


class ExperimentController:
    def __init__(self, serial_exp):
        self.serial = serial_exp
        self.stop_event = threading.Event()
        self.worker = None
        self.is_running = False

    def start(self, config: dict, log_callback, state_callback):
        if self.is_running:
            return

        self.validate_config(config)

        self.stop_event.clear()
        self.is_running = True
        state_callback(True)

        self.worker = threading.Thread(
            target=self._run,
            args=(config, log_callback, state_callback),
            daemon=True,
        )
        self.worker.start()

    def stop(self):
        self.stop_event.set()

    def validate_config(self, config: dict):
        port = config["port"].strip()
        path = config["filepath"].strip()

        if not port:
            raise ValueError("Port is required.")
        if not path:
            raise ValueError("CSV file path is required.")

        run_mode = config["run_mode"]
        if run_mode not in {"all", "list", "except"}:
            raise ValueError("Invalid run mode.")

        if run_mode in {"list", "except"}:
            systems = config["systems"]
            if not systems:
                raise ValueError("Choose at least one system.")
            if len(set(systems)) != len(systems):
                raise ValueError("Duplicate system selections are not allowed.")
            for s in systems:
                if s < 0 or s > 7:
                    raise ValueError("Systems must be in 0..7.")

        global_brightness = config["global_brightness"]
        if global_brightness is not None:
            if len(global_brightness) != 5:
                raise ValueError("Global brightness must have 5 values.")
            for v in global_brightness:
                if v < 0 or v > 255:
                    raise ValueError("Global brightness values must be 0..255.")

        module_brightness = config["module_brightness"]
        for module_idx, vals in module_brightness.items():
            if module_idx < 0 or module_idx > 7:
                raise ValueError("Module index must be in 0..7.")
            if len(vals) != 5:
                raise ValueError(f"Module {module_idx} brightness must have 5 values.")
            for v in vals:
                if v < 0 or v > 255:
                    raise ValueError(f"Module {module_idx} brightness values must be 0..255.")

    def _send_run_selection(self, config, log):
        run_mode = config["run_mode"]
        systems = config["systems"]

        if run_mode == "all":
            ok = self.serial.send_cmd_and_wait_ok("LIST=ALL", log_callback=log)
            if not ok:
                raise RuntimeError("Failed to send LIST=ALL")

        elif run_mode == "list":
            cmd = "LIST=" + ",".join(str(x) for x in systems)
            ok = self.serial.send_cmd_and_wait_ok(cmd, log_callback=log)
            if not ok:
                raise RuntimeError(f"Failed to send {cmd}")

        elif run_mode == "except":
            cmd = "EXCEPT=" + ",".join(str(x) for x in systems)
            ok = self.serial.send_cmd_and_wait_ok(cmd, log_callback=log)
            if not ok:
                raise RuntimeError(f"Failed to send {cmd}")

    def _send_brightness(self, config, log):
        global_brightness = config["global_brightness"]
        module_brightness = config["module_brightness"]

        if global_brightness is not None:
            b = global_brightness
            cmd = f"B={b[0]},{b[1]},{b[2]},{b[3]},{b[4]}"
            ok = self.serial.send_cmd_and_wait_ok(cmd, log_callback=log)
            if not ok:
                raise RuntimeError(f"Failed to send {cmd}")

        for module_idx in sorted(module_brightness.keys()):
            b = module_brightness[module_idx]
            cmd = f"BM{module_idx}={b[0]},{b[1]},{b[2]},{b[3]},{b[4]}"
            ok = self.serial.send_cmd_and_wait_ok(cmd, log_callback=log)
            if not ok:
                raise RuntimeError(f"Failed to send {cmd}")

    def _run(self, config, log, state_callback):
        file_handle = None

        try:
            port = config["port"].strip()
            filepath = Path(config["filepath"]).expanduser()
            filepath.parent.mkdir(parents=True, exist_ok=True)

            log(f"Opening serial: {port}")
            self.serial.connect(port)

            # Drain startup chatter after board reset / reconnect
            self.serial.drain_serial(duration_s=5.0, echo_callback=log)

            if not self.serial.wait_for_status_response(log_callback=log):
                raise RuntimeError("Board did not respond to STATUS.")

            # Let any remaining status lines finish printing, then clear the buffer
            self.serial.drain_serial(duration_s=1.5, echo_callback=log)
            self.serial.ser.reset_input_buffer()

            self._send_run_selection(config, log)
            time.sleep(0.2)
            self.serial.drain_serial(duration_s=0.2, echo_callback=log)

            self._send_brightness(config, log)
            time.sleep(0.2)
            self.serial.drain_serial(duration_s=0.2, echo_callback=log)

            file_handle = filepath.open("w", newline="")
            file_handle.write(CSV_HEADER + "\n")
            file_handle.flush()
            log(f"Writing to: {filepath}")

            time.sleep(0.2)
            self.serial.drain_serial(duration_s=0.2, echo_callback=log)

            ok = self.serial.send_cmd_and_wait_ok("START", log_callback=log)
            if not ok:
                raise RuntimeError("Failed to START.")

            rows = 0

            while not self.stop_event.is_set():
                line = self.serial.read_line()
                if not line:
                    continue

                if self.serial.is_chatter_line(line):
                    continue

                if line.startswith("time(ms),"):
                    continue

                parts = line.split(",")
                if not self.serial.looks_like_data_row(parts):
                    continue

                file_handle.write(line + "\n")
                rows += 1

                if rows % 50 == 0:
                    file_handle.flush()
                    log(f"Wrote {rows} rows")

            ok = self.serial.send_cmd_and_wait_ok("STOP", timeout_s=3.0, log_callback=log)
            if ok:
                log("STOP acknowledged.")
            else:
                log("STOP sent, no OK received.")

        except Exception as e:
            log(f"ERROR: {e}")

        finally:
            try:
                if file_handle is not None:
                    file_handle.flush()
                    file_handle.close()
            except Exception:
                pass

            self.serial.disconnect()
            self.stop_event.clear()
            self.is_running = False
            state_callback(False)
            log("Stopped.")
