#!/bin/bash
# ========================================================
# Distributed Cluster Monitoring System - Test Script
# ========================================================

PORT=5050  # ← CHANGED from 6000 to match your code
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

echo "=== Cleaning old processes and logs ==="
pkill -9 -f "./manager" 2>/dev/null
pkill -9 -f "./worker" 2>/dev/null
sleep 1
rm -f cluster_state.json
rm -f $LOG_DIR/*.log
rm -f manager.log worker.log

# -------------------------------
# 1️⃣ Start Primary Manager
# -------------------------------
echo "=== Starting Primary Manager on port $PORT ==="
./manager primary > $LOG_DIR/manager.log 2>&1 &
MANAGER_PID=$!
echo "Manager PID: $MANAGER_PID"
sleep 3

# Check if manager started
if ! ps -p $MANAGER_PID > /dev/null; then
    echo "ERROR: Manager failed to start. Check logs/manager.log"
    cat $LOG_DIR/manager.log
    exit 1
fi

# -------------------------------
# 2️⃣ Start Workers
# -------------------------------
echo "=== Starting 2 worker nodes ==="
./worker node1 > $LOG_DIR/worker_node1.log 2>&1 &
WORKER1_PID=$!
./worker node2 > $LOG_DIR/worker_node2.log 2>&1 &
WORKER2_PID=$!
echo "Worker PIDs: $WORKER1_PID, $WORKER2_PID"
sleep 8

echo ""
echo "=== Checking registration and heartbeat logs ==="
echo "--- Manager log (registrations) ---"
grep "REGISTER" $LOG_DIR/manager.log 2>/dev/null | tail -n 5
echo ""
echo "--- Worker node1 heartbeats ---"
grep "Heartbeat sent" $LOG_DIR/worker_node1.log 2>/dev/null | tail -n 3
echo ""

# -------------------------------
# 3️⃣ Simulate Node Failure
# -------------------------------
echo "=== Simulating node2 failure (kill node2 process) ==="
kill -9 $WORKER2_PID 2>/dev/null
echo "Waiting 15 seconds for failure detection..."
sleep 15

echo ""
echo "=== Checking for failure detection ==="
grep "failed" $LOG_DIR/manager.log 2>/dev/null | tail -n 5
echo ""

# -------------------------------
# 4️⃣ Test Manager Failover (Primary -> Backup)
# -------------------------------
echo "=== Starting Backup Manager ==="
./manager backup > $LOG_DIR/manager_backup.log 2>&1 &
BACKUP_PID=$!
sleep 3

echo "=== Killing Primary Manager to test failover ==="
kill -9 $MANAGER_PID 2>/dev/null
echo "Waiting for backup takeover..."
sleep 5

echo ""
echo "=== Checking backup takeover logs ==="
grep "Taking over" $LOG_DIR/manager_backup.log 2>/dev/null
echo ""

echo "=== Checking if node1 reconnected to backup ==="
sleep 5
grep "Reconnected" $LOG_DIR/worker_node1.log 2>/dev/null | tail -n 2
echo ""

# -------------------------------
# 5️⃣ Verify Persistence
# -------------------------------
echo "=== Checking cluster state persistence ==="
if [ -f cluster_state.json ]; then
    echo "cluster_state.json contents:"
    cat cluster_state.json
else
    echo "WARNING: cluster_state.json not found"
fi
echo ""

# -------------------------------
# 6️⃣ Clean Exit
# -------------------------------
echo "=== Cleaning up all processes ==="
kill -9 $BACKUP_PID 2>/dev/null
kill -9 $WORKER1_PID 2>/dev/null
pkill -9 -f "./manager" 2>/dev/null
pkill -9 -f "./worker" 2>/dev/null

echo ""
echo "=== Test Complete! ==="
echo "Logs saved in: $LOG_DIR/"
echo "--------------------------------------------------"
echo "Summary of what was tested:"
echo "  ✓ Manager startup and worker registration"
echo "  ✓ Heartbeat mechanism"
echo "  ✓ Node failure detection"
echo "  ✓ Primary-to-Backup failover"
echo "  ✓ Worker reconnection to backup"
echo "  ✓ State persistence"
echo "--------------------------------------------------"
echo ""
echo "Inspect these logs for details:"
echo "  - logs/manager.log (primary manager)"
echo "  - logs/manager_backup.log (backup manager takeover)"
echo "  - logs/worker_node1.log (worker that survived)"
echo "  - logs/worker_node2.log (worker that was killed)"
echo "--------------------------------------------------"