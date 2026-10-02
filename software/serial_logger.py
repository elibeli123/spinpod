'''
This program provides the serial communication layer between the host computer and the embedded experiment hardware. 
It opens and closes the serial connection, sends control commands such as STATUS or START, waits for valid responses from the microcontroller, 
and retries certain commands if a transient communication error occurs. It also filters out startup chatter and non-data messages so the system 
can distinguish status text from actual measurement rows. In addition, it validates whether incoming comma-separated lines match the expected 
spectral data format before they are passed on for logging or analysis.
'''

import serial
import time

BAUD = 115200
EXPECTED_COLS = 21
CSV_HEADER = (
    "time(ms),sensor_id,trial,410nm,435nm,460nm,485nm,510nm,535nm,"
    "560nm,585nm,610nm,645nm,680nm,705nm,730nm,760nm,810nm,860nm,900nm,940nm"
)


class SerialExperiment:
    def __init__(self):
        self.ser = None

    def connect(self, port: str):
        self.ser = serial.Serial(port, BAUD, timeout=2)
        time.sleep(4.0)

    def disconnect(self):
        if self.ser is not None:
            try:
                self.ser.close()
            finally:
                self.ser = None

    def read_line(self) -> str:
        raw = self.ser.readline()
        if not raw:
            return ""
        return raw.decode("utf-8", errors="replace").strip()

    def drain_serial(self, duration_s: float = 5.0, echo_callback=None):
        """
        Drain startup chatter after opening the serial port.
        This is mainly used to consume Arduino boot text.
        """
        t0 = time.time()
        while time.time() - t0 < duration_s:
            line = self.read_line()
            if line and echo_callback:
                echo_callback(f"drain: {line}")

    def wait_for_status_response(self, timeout_s: float = 15.0, log_callback=None) -> bool:
        """
        Send STATUS and wait until we see a valid status block.
        This version does not rely on leading spaces, because read_line()
        uses .strip() and removes indentation.
        """
        self.send_raw("STATUS")
        t0 = time.time()

        saw_status_header = False
        saw_started = False
        saw_runlist = False
        saw_brightness = False
        saw_commands = False

        while time.time() - t0 < timeout_s:
            line = self.read_line()
            if not line:
                continue

            if log_callback:
                log_callback(f"status: {line}")

            if line.startswith("=== STATUS ==="):
                saw_status_header = True
                continue

            if line.startswith("started="):
                saw_started = True
                continue

            if line.startswith("runList="):
                saw_runlist = True
                continue

            if line.startswith("brightness5="):
                saw_brightness = True
                continue

            if line.startswith("Commands:"):
                saw_commands = True
                continue

            # expected extra status lines
            if line.startswith("Sensor CH"):
                continue

            if saw_status_header and saw_started and saw_runlist and saw_brightness and saw_commands:
                return True

        return saw_status_header and saw_started and saw_runlist and saw_brightness and saw_commands

    def send_raw(self, cmd: str):
        self.ser.write((cmd.strip() + "\n").encode("utf-8"))
        self.ser.flush()

    def send_cmd_and_wait_ok(self, cmd: str, timeout_s: float = 5.0, log_callback=None) -> bool:
        for attempt in range(2):
            if log_callback:
                suffix = "" if attempt == 0 else " (retry)"
                log_callback(f"Sending: {cmd}{suffix}")

            self.send_raw(cmd)
            t0 = time.time()

            while time.time() - t0 < timeout_s:
                line = self.read_line()
                if not line:
                    continue

                if log_callback:
                    log_callback(f"recv: {line}")

                if line.startswith("OK"):
                    return True

                if line.startswith("ERR"):
                    # Retry once if START got a transient malformed parse
                    if cmd == "START" and "unknown cmd" in line and attempt == 0:
                        time.sleep(0.2)
                        self.drain_serial(duration_s=0.2, echo_callback=log_callback)
                        break
                    return False

        return False

    @staticmethod
    def looks_like_data_row(parts) -> bool:
        if len(parts) != EXPECTED_COLS:
            return False

        try:
            int(parts[0])
            int(parts[1])
        except ValueError:
            return False

        if not parts[2]:
            return False

        for x in parts[3:]:
            try:
                float(x)
            except ValueError:
                return False

        return True

    @staticmethod
    def is_chatter_line(line: str) -> bool:
        if not line:
            return True

        if line.startswith("----") or line.startswith("==="):
            return True

        if line.startswith("started=") or line.startswith("nSystems=") or line.startswith("brightness5="):
            return True

        if line.startswith("runList="):
            return True

        if line.startswith("Sensor CH") and (" init OK" in line or " init FAIL" in line):
            return True
        if line.startswith("Sensor CH") and (": OK" in line or ": FAIL" in line):
            return True

        if line.startswith("Commands:"):
            return True

        if line.startswith("OK") or line.startswith("WARN") or line.startswith("ERR"):
            return True

        if "," not in line:
            return True

        return False
