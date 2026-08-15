import json
import logging
import os
import signal
import sys
import time
from pathlib import Path

from dotenv import load_dotenv
import serial
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import ASYNCHRONOUS

# Resolve paths relative to this script so the service works from any cwd
SCRIPT_DIR = Path(__file__).resolve().parent
LOG_FILE = SCRIPT_DIR / "esp32_influx.log"

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler(), logging.FileHandler(LOG_FILE)],
)
logger = logging.getLogger(__name__)

# Load .env from the same directory as this script
load_dotenv(SCRIPT_DIR / ".env")

SERIAL_PORT = os.getenv("SERIAL_PORT", "/dev/cu.usbmodem1101")
BAUD_RATE = int(os.getenv("BAUD_RATE", "115200"))

INFLUX_URL = os.getenv("INFLUX_URL", "http://localhost:8086")
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN")
INFLUX_ORG = os.getenv("INFLUX_ORG", "personal")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET", "weather-station")
WRITE_INTERVAL = int(os.getenv("WRITE_INTERVAL", "60"))  # seconds between InfluxDB writes

if not INFLUX_TOKEN:
    raise ValueError("INFLUX_TOKEN environment variable must be set")

try:
    client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    # Use ASYNCHRONOUS to avoid blocking the main serial loop during network drops
    write_api = client.write_api(write_options=ASYNCHRONOUS)
except Exception as e:
    raise ConnectionError(f"Failed to connect to InfluxDB: {e}")


def connect_serial():
    while True:
        try:
            ser = serial.Serial(
                port=SERIAL_PORT,
                baudrate=BAUD_RATE,
                timeout=2,
                write_timeout=2,
                dsrdtr=False,
                rtscts=False,
            )
            # Flush partial bytes sitting in the buffer from boot/reconnects
            ser.reset_input_buffer()
            logger.info(f"Successfully connected to {SERIAL_PORT}")
            return ser
        except serial.SerialException as e:
            logger.warning(f"Serial port error: {e}. Retrying in 2 seconds...")
            time.sleep(2)
        except Exception as e:
            logger.error(f"Unexpected error: {e}")
            time.sleep(2)


ser = connect_serial()
running = True
last_write = 0.0


def handle_shutdown(sig, frame):
    global running
    logger.info("Shutdown signal received. Cleaning up...")
    running = False


signal.signal(signal.SIGINT, handle_shutdown)
signal.signal(signal.SIGTERM, handle_shutdown)

while running:
    try:
        raw_line = ser.readline()
        if not raw_line:
            continue

        line = raw_line.decode("utf-8", errors="ignore").strip()
        if not (line.startswith("{") and line.endswith("}")):
            continue

        data = json.loads(line)

        if all(k in data for k in ("temperature", "humidity", "pressure")):
            now = time.monotonic()
            if now - last_write < WRITE_INTERVAL:
                continue

            last_write = now
            point = (
                Point("bme280_telemetry")
                .tag("location", "desk")
                .field("temperature", float(data["temperature"]))
                .field("humidity", float(data["humidity"]))
                .field("pressure", float(data["pressure"]))
            )

            write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
            logger.info(
                f"Logged data - Temp: {float(data['temperature']):.2f}°C | "
                f"Humidity: {float(data['humidity']):.2f}% | "
                f"Pressure: {float(data['pressure']):.2f}hPa"
            )

    except (serial.SerialException, serial.SerialTimeoutException) as e:
        logger.error(f"Serial connection lost: {e}. Reconnecting...")
        try:
            ser.close()
        except Exception:
            pass
        time.sleep(1)
        if running:
            ser = connect_serial()
    except json.JSONDecodeError:
        logger.debug(f"Invalid JSON received: {line}")
    except Exception as e:
        logger.error(f"Unexpected error: {e}", exc_info=True)
        time.sleep(1)

# Resource cleanup
try:
    ser.close()
    write_api.close()
    client.close()
    logger.info("Closed connections cleanly.")
except Exception:
    pass
sys.exit(0)
