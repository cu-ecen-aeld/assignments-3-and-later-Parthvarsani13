#!/bin/sh

### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:
# Required-Stop:
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start the aesdsocket daemon
### END INIT INFO

COMMAND=$1
DAEMON_NAME="aesdsocket"
DAEMON_PATH="/usr/bin/aesdsocket"
DAEMON_OPTS="-d"


if [ -z "$COMMAND" ]; then
    echo "Usage: $0 start|stop"
    exit 1
fi

case "$COMMAND" in
    start)
        echo "Launching ${DAEMON_NAME}..."
        if start-stop-daemon -S -n ${DAEMON_NAME} -a ${DAEMON_PATH} -- -d; then
            echo "${DAEMON_NAME} started successfully."
        else
            echo "Failed to launch ${DAEMON_NAME}."
            exit 1
        fi
        ;;

    stop)
        echo "Shutting down ${DAEMON_NAME}..."
        if start-stop-daemon -K -n ${DAEMON_NAME} --signal SIGTERM; then
            echo "${DAEMON_NAME} stopped."
        else
            echo "Failed to stop ${DAEMON_NAME}."
            exit 1
        fi
        ;;

    *)
        echo "Invalid command. Usage: $0 start|stop"
        exit 1
        ;;
esac

exit 0
