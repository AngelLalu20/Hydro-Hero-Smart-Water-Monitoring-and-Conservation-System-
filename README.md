# Hydro-Hero Smart Water Monitoring System

**README – Code Explanation**

## 1. Project Overview

Hydro-Hero is an embedded IoT-based smart water monitoring and conservation system built on the ESP32 platform. The firmware performs real-time flow sensing, consumption analytics, anomaly detection, alert generation, and remote dashboard monitoring through a built-in web server.

The system is designed to operate entirely on-device without requiring external cloud services. It provides real-time insights into water usage, leak detection, and system health.

---

## 2. Main Features

* Real-time water flow measurement
* Daily, weekly, monthly, and yearly usage tracking
* Statistical analytics and trend detection
* Leak and anomaly detection
* Water quality monitoring (simulated sensors)
* Budget and cost estimation
* Multi-zone water monitoring
* Automated valve control
* Built-in web dashboard
* Task scheduler for periodic operations
* System health monitoring

---

## 3. Hardware Requirements

* ESP32 microcontroller
* Water flow sensor (pulse-based)
* Relay-controlled solenoid valve
* Buzzer for alerts
* Status LED
* Temperature sensor (analog)

---

## 4. Software Architecture

The firmware is organized into modular subsystems:

### 4.1 Flow Measurement Module

Responsible for:

* Counting pulses from the flow sensor
* Converting pulses into:

  * Instantaneous flow rate (L/min)
  * Total water consumption
* Updating daily, weekly, monthly, and yearly counters

Key function:

```
updateFlowData()
```

Interrupt handler:

```
pulseCounter()
```

---

### 4.2 Statistics Module

Calculates:

* Average flow
* Peak and minimum flow
* Usage percentiles
* Weekly trend analysis

Key function:

```
updateStatistics()
```

---

### 4.3 Prediction Module

Implements a weighted moving average model to estimate:

* Daily consumption
* Weekly consumption
* Monthly consumption
* Seasonal adjustments

Key function:

```
updatePredictions()
```

---

### 4.4 Alert Engine

Detects abnormal conditions:

* High flow
* Continuous low-level leaks
* Sudden flow drops
* No-usage conditions
* Water quality violations
* Budget exceedance
* System health issues

Alerts are stored in a circular buffer and categorized by severity.

Key function:

```
checkAlerts()
```

---

### 4.5 Water Quality Module

Simulates water quality parameters:

* Temperature
* pH level
* Turbidity
* Conductivity
* Chlorine concentration

Key function:

```
updateWaterQuality()
```

---

### 4.6 Billing Module

Calculates:

* Daily cost
* Monthly cost
* Total cost
* Carbon footprint

Based on:

```
ratePer1000L
```

Key function:

```
updateBilling()
```

---

### 4.7 Zone Management Module

Supports multiple monitoring zones such as:

* Kitchen
* Bathroom
* Garden
* Laundry

Each zone:

* Tracks its own usage
* Detects leaks independently

Key functions:

```
initializeZones()
updateZoneData()
```

---

### 4.8 Web Dashboard Module

A built-in web server provides:

* Real-time flow data
* Alerts and system status
* Water quality metrics
* Billing information
* Control interface for valve

Main handlers:

```
handleRoot()
handleAPIStatus()
handleAlertsPage()
```

---

### 4.9 System Health Module

Monitors:

* Free memory
* Wi-Fi signal strength
* CPU load (simulated)
* System uptime

Key function:

```
checkSystemHealth()
```

---

### 4.10 Task Scheduler

Implements a cooperative task scheduler.

Each task has:

* Name
* Interval
* Last execution time
* Function pointer

Main functions:

```
addTask()
runScheduledTasks()
```

Scheduled tasks include:

* Flow updates
* Alerts
* Statistics
* Predictions
* Water quality
* Billing
* LED status
* Data backup

---

## 5. Data Storage

The system uses:

### EEPROM

Stores:

* Thresholds
* Configuration
* Device information
* Total consumption

Functions:

```
saveConfig()
loadConfig()
```

### SPIFFS

Used for:

* Data logging
* Backup storage

---

## 6. Main Program Flow

### setup()

Initializes:

* Serial communication
* Hardware pins
* EEPROM and configuration
* Wi-Fi connection
* Web server
* Zones
* Scheduled tasks

### loop()

Runs continuously:

1. Handles web requests
2. Executes scheduled tasks
3. Performs daily resets
4. Processes serial commands

---

## 7. Key API Endpoints

| Endpoint            | Function           |
| ------------------- | ------------------ |
| `/`                 | Main dashboard     |
| `/api/status`       | JSON system status |
| `/api/valve/toggle` | Toggle valve       |
| `/alerts`           | Alerts page        |

---

## 8. Serial Commands (Development)

Available commands:

```
reset       → Reset usage counters
valve on    → Open valve
valve off   → Close valve
status      → Print system status
```

---

## 9. Technologies Used

* Embedded C/C++
* Arduino framework
* ESP32 microcontroller
* Wi-Fi networking
* Web server interface
* EEPROM storage
* SPIFFS file system
* JSON data exchange

---

## 10. Authors

Angel Lalu
Divya Bharti
Shreyas S

---

## 11. License

This software is an original, unpublished work protected under the Copyright Act, 1957 (India).
All rights reserved.

---
