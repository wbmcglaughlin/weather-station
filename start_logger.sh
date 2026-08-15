#!/bin/bash
# Wrapper script for launchd — runs logger.py using the project's venv.
# launchd does not source shell profiles, so all paths must be absolute.

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

exec "$PROJECT_DIR/.venv/bin/python" "$PROJECT_DIR/logger.py"
