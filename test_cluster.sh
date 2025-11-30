#!/bin/bash
# ========================================================
# Distributed Cluster Monitoring System - Test Script
# ========================================================
# This script automates testing of:
# - Manager startup
# - Worker registration and heartbeat
# - Failure detection
# - State persistence across restarts
# ========================================================

PORT=6000
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

echo "=== Cleaning old processes and logs ==="
pkill -f manager 2>/dev/null
pkill -f worker 2>/dev/null
rm -f cluster_state.json
rm -f $LOG_DIR/*.log

# -------------------------------
# 1️⃣ Start Manager
# -------------------------------
echo "=== Starting Manager on port $PORT ==="
./manager primary > $LOG_DIR/manager.log 2>&1 &
MANAGER_PID=$!
sleep 2

# -------------------------------
# 2️⃣ Start Workers
# -------------------------------
echo "=== Starting 3 worker nodes ==="
./worker node1 > $LOG_DIR/worker_node1.log 2>&1 &
./worker node2 > $LOG_DIR/worker_node2.log 2>&1 &
./worker node3 > $LOG_DIR/worker_node3.log 2>&1 &
sleep 10

echo "=== Checking registration and heartbeat logs ==="
grep "REGISTER" $LOG_DIR/manager.log | tail -n 5
grep "Heartbeat sent" $LOG_DIR/worker_node1.log | tail -n 2

# -------------------------------
# 3️⃣ Simulate Node Failure
# -------------------------------
echo "=== Simulating node2 failure (kill node2 process) ==="
pkill -f "worker node2"
sleep 15

echo "=== Checking for failure detection ==="
grep "failed" $LOG_DIR/manager.log | tail -n 5

# -------------------------------
# 4️⃣ Restart Manager to test persistence
# -------------------------------
echo "=== Restarting Manager to verify persistent state ==="
kill $MANAGER_PID
sleep 2
./manager primary > $LOG_DIR/manager_restart.log 2>&1 &
MANAGER_PID=$!
sleep 5

echo "=== Checking loaded cluster state ==="
grep "Cluster state loaded" $LOG_DIR/manager_restart.log
grep "active" cluster_state.json | head -n 10

# -------------------------------
# 5️⃣ Clean Exit
# -------------------------------
echo "=== Cleaning up all processes ==="
kill $MANAGER_PID
pkill -f worker

echo "=== Test Complete! ==="
echo "Logs saved in: $LOG_DIR/"
echo "--------------------------------------------------"
echo "Inspect manager.log and worker_node*.log for details"
echo "--------------------------------------------------"
