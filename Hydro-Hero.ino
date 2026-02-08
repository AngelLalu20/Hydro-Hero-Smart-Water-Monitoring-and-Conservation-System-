#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPIFFS.h>

// Pin definitions
#define FLOW_SENSOR_PIN 27
#define VALVE_RELAY_PIN 26
#define BUZZER_PIN 25
#define STATUS_LED_PIN 33
#define TEMP_SENSOR_PIN 32

// Configuration
#define EEPROM_SIZE 512
#define HISTORY_SIZE 1008  // 7 days * 24 hours * 6 (10-minute intervals)
#define MAX_EVENTS 100
#define MAX_ZONES 5
#define UPDATE_INTERVAL 1000  // 1 second

WebServer server(80);

// WiFi credentials - stored in EEPROM
String ssid = "XXXXXX";
String password = "YYYYYY";

// Device identity
struct DeviceInfo {
  String name = "Hydro-Hero Node";
  String firmwareVersion = "4.0.0";
  String location = "Main Supply Line";
  String deviceID;
  String timezone = "GMT-5";
};

DeviceInfo device;

// Time management
struct TimeInfo {
  time_t now;
  struct tm timeinfo;
  bool timeSynced = false;
  unsigned long lastSync = 0;
  const unsigned long syncInterval = 3600000;  // 1 hour
};

TimeInfo timeInfo;

// Flow measurement
struct FlowData {
  volatile unsigned long pulseCount = 0;
  float pulsesPerLitre = 450.0;
  float flowRate = 0.0;           // L/min
  float instantaneousFlow = 0.0;  // L/s
  float totalLitres = 0.0;
  float dailyLitres = 0.0;
  float weeklyLitres = 0.0;
  float monthlyLitres = 0.0;
  float yearlyLitres = 0.0;
  
  // Hourly tracking
  float hourlyUsage[24] = {0};
  int currentHour = 0;
  
  // Historical data buffers
  float flowHistory[HISTORY_SIZE];
  float pressureHistory[HISTORY_SIZE];  // Simulated pressure
  uint32_t historyTimestamps[HISTORY_SIZE];
  int historyIndex = 0;
};

FlowData flow;

// Statistics
struct Statistics {
  float peakFlow = 0;
  float minFlow = 9999;
  float averageFlow = 0;
  float medianFlow = 0;
  float standardDeviation = 0;
  unsigned long sampleCount = 0;
  float flowPercentiles[5] = {0};  // 10th, 25th, 50th, 75th, 90th
  
  // Usage patterns
  float dailyAverage = 0;
  float weeklyAverage = 0;
  float monthlyAverage = 0;
  float usageTrend = 0;  // Percentage change
};

Statistics stats;

// Prediction model
struct Predictions {
  float predictedDailyUsage = 0;
  float predictedWeeklyUsage = 0;
  float predictedMonthlyUsage = 0;
  float seasonalFactor = 1.0;
  float trendFactor = 1.0;
  float confidenceScore = 0.0;
  
  // Machine learning-like weights (simplified)
  float weights[7] = {0.15, 0.20, 0.25, 0.15, 0.10, 0.10, 0.05};  // Last 7 days weights
};

Predictions predictions;

// Alert system
struct Alert {
  String type;
  String message;
  unsigned long timestamp;
  int severity;  // 1=Low, 2=Medium, 3=High, 4=Critical
  bool acknowledged;
  bool active;
};

#define MAX_ALERTS 50
Alert alerts[MAX_ALERTS];
int alertIndex = 0;

struct AlertThresholds {
  float maxFlowThreshold = 10.0;    // L/min
  float leakThreshold = 0.2;        // L/min
  float suddenDropThreshold = 5.0;  // Percentage
  float noUsageThreshold = 24.0;    // Hours
  float pressureDropThreshold = 10.0; // Percentage
  float temperatureThreshold = 80.0;  // °F
  
  // Anomaly detection
  float anomalyThreshold = 2.0;     // Standard deviations
  unsigned long anomalyWindow = 3600000;  // 1 hour
};

AlertThresholds thresholds;

// Zone management
struct Zone {
  String name;
  int sensorPin;
  bool enabled;
  float flowRate;
  float totalUsage;
  float dailyUsage;
  bool leakDetected;
  unsigned long leakStartTime;
  float customThreshold;
};

Zone zones[MAX_ZONES];
int zoneCount = 0;

// System configuration
struct SystemConfig {
  bool autoValveShutdown = true;
  bool emailAlerts = false;
  bool smsAlerts = false;
  bool soundAlerts = true;
  bool ledIndicators = true;
  bool dataLogging = true;
  bool powerSaveMode = false;
  int loggingInterval = 300;  // 5 minutes
  int reportInterval = 3600;  // 1 hour
  String apiKey = "";
  String mqttServer = "";
  int mqttPort = 1883;
  String webhookURL = "";
};

SystemConfig config;

// System health
struct SystemHealth {
  float cpuLoad = 0;
  float memoryUsage = 0;
  float uptime = 0;
  int wifiSignal = 0;
  float voltage = 3.3;
  float temperature = 0;
  int totalRestarts = 0;
  unsigned long lastRestart = 0;
  String lastError = "";
  int errorCount = 0;
};

SystemHealth health;

// Water quality (simulated sensors)
struct WaterQuality {
  float temperature = 0;      // °C
  float pH = 7.0;             // pH level
  float turbidity = 0;        // NTU
  float conductivity = 0;     // μS/cm
  float chlorine = 0;         // mg/L
  bool qualityAlert = false;
  unsigned long lastCheck = 0;
};

WaterQuality waterQuality;

// Billing and cost
struct BillingInfo {
  float ratePer1000L = 2.50;  // $ per 1000 liters
  float currentCost = 0;
  float dailyCost = 0;
  float monthlyCost = 0;
  float yearlyCost = 0;
  float budget = 100.0;
  bool budgetAlert = false;
  float carbonFootprint = 0;  // kg CO2 equivalent
};

BillingInfo billing;

// Event logging
struct Event {
  String type;
  String message;
  unsigned long timestamp;
  int priority;
  String source;
};

Event events[MAX_EVENTS];
int eventIndex = 0;

// Task scheduler
struct ScheduledTask {
  String name;
  unsigned long interval;
  unsigned long lastRun;
  bool enabled;
  void (*function)();
};

#define MAX_TASKS 20
ScheduledTask tasks[MAX_TASKS];
int taskCount = 0;

// Forward declarations
void IRAM_ATTR pulseCounter();
void updateFlowData();
void checkAlerts();
void updateStatistics();
void updatePredictions();
void updateWaterQuality();
void updateBilling();
void logEvent(String type, String message, int priority = 1, String source = "System");
void addAlert(String type, String message, int severity);
void handleValve(bool state);
void playAlertTone(int pattern);
void updateLEDStatus();
void saveToHistory();
void generateReport();
void backupData();
void syncTime();
void sendNotification(String message);
void initializeZones();
void updateZoneData();
void checkSystemHealth();

// ============ EEPROM Management ============
void saveConfig() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = 0;
  
  // Save thresholds
  EEPROM.put(addr, thresholds);
  addr += sizeof(thresholds);
  
  // Save config
  EEPROM.put(addr, config);
  addr += sizeof(config);
  
  // Save device info
  EEPROM.writeString(addr, device.name);
  addr += device.name.length() + 1;
  EEPROM.writeString(addr, device.location);
  addr += device.location.length() + 1;
  EEPROM.writeString(addr, device.timezone);
  addr += device.timezone.length() + 1;
  
  // Save totals
  EEPROM.put(addr, flow.totalLitres);
  addr += sizeof(flow.totalLitres);
  
  EEPROM.commit();
  EEPROM.end();
  
  logEvent("Config", "Configuration saved", 2, "System");
}

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = 0;
  
  // Load thresholds
  EEPROM.get(addr, thresholds);
  addr += sizeof(thresholds);
  
  // Load config
  EEPROM.get(addr, config);
  addr += sizeof(config);
  
  // Load device info
  device.name = EEPROM.readString(addr);
  addr += device.name.length() + 1;
  device.location = EEPROM.readString(addr);
  addr += device.location.length() + 1;
  device.timezone = EEPROM.readString(addr);
  addr += device.timezone.length() + 1;
  
  // Load totals
  EEPROM.get(addr, flow.totalLitres);
  addr += sizeof(flow.totalLitres);
  
  EEPROM.end();
  
  // Generate device ID from MAC address
  uint8_t mac[6];
  esp_read_mac(mac);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  device.deviceID = String(macStr);
  
  logEvent("Config", "Configuration loaded", 2, "System");
}

// ============ Event Logging ============
void logEvent(String type, String message, int priority, String source) {
  events[eventIndex].type = type;
  events[eventIndex].message = message;
  events[eventIndex].timestamp = millis();
  events[eventIndex].priority = priority;
  events[eventIndex].source = source;
  
  eventIndex = (eventIndex + 1) % MAX_EVENTS;
  
  // Also print to serial for debugging
  if (priority >= 2) {  // Medium priority and above
    Serial.printf("[%s] %s: %s\n", source.c_str(), type.c_str(), message.c_str());
  }
}

// ============ Alert Management ============
void addAlert(String type, String message, int severity) {
  // Check if similar alert already exists
  for (int i = 0; i < MAX_ALERTS; i++) {
    if (alerts[i].active && alerts[i].type == type && 
        alerts[i].message == message) {
      return;  // Alert already exists
    }
  }
  
  alerts[alertIndex].type = type;
  alerts[alertIndex].message = message;
  alerts[alertIndex].timestamp = millis();
  alerts[alertIndex].severity = severity;
  alerts[alertIndex].acknowledged = false;
  alerts[alertIndex].active = true;
  
  alertIndex = (alertIndex + 1) % MAX_ALERTS;
  
  // Trigger notifications based on severity
  if (severity >= 3) {  // High or Critical
    if (config.soundAlerts) playAlertTone(severity);
    if (config.emailAlerts) sendNotification("ALERT: " + message);
    if (config.smsAlerts) sendNotification("SMS: " + message);
  }
  
  logEvent("Alert", message, severity, type);
}

void acknowledgeAlert(int index) {
  if (index >= 0 && index < MAX_ALERTS) {
    alerts[index].acknowledged = true;
  }
}

void clearAlert(int index) {
  if (index >= 0 && index < MAX_ALERTS) {
    alerts[index].active = false;
  }
}

// ============ Advanced Statistics ============
void updateStatistics() {
  // Update basic stats
  stats.sampleCount++;
  stats.averageFlow = ((stats.averageFlow * (stats.sampleCount - 1)) + flow.flowRate) / stats.sampleCount;
  
  if (flow.flowRate > stats.peakFlow) stats.peakFlow = flow.flowRate;
  if (flow.flowRate < stats.minFlow && flow.flowRate > 0) stats.minFlow = flow.flowRate;
  
  // Update hourly usage array
  if (timeInfo.timeSynced) {
    int hour = timeInfo.timeinfo.tm_hour;
    if (hour != flow.currentHour) {
      flow.currentHour = hour;
      flow.hourlyUsage[hour] = 0;
    }
    flow.hourlyUsage[hour] += flow.flowRate / 60.0;  // Convert L/min to L for this minute
  }
  
  // Calculate percentiles (simplified)
  static float flowBuffer[100];
  static int bufferIndex = 0;
  
  flowBuffer[bufferIndex] = flow.flowRate;
  bufferIndex = (bufferIndex + 1) % 100;
  
  if (bufferIndex == 0) {
    // Simple percentile calculation
    float sorted[100];
    memcpy(sorted, flowBuffer, sizeof(flowBuffer));
    
    // Simple bubble sort for demonstration
    for (int i = 0; i < 99; i++) {
      for (int j = 0; j < 99 - i; j++) {
        if (sorted[j] > sorted[j + 1]) {
          float temp = sorted[j];
          sorted[j] = sorted[j + 1];
          sorted[j + 1] = temp;
        }
      }
    }
    
    stats.flowPercentiles[0] = sorted[9];   // 10th
    stats.flowPercentiles[1] = sorted[24];  // 25th
    stats.flowPercentiles[2] = sorted[49];  // 50th (median)
    stats.flowPercentiles[3] = sorted[74];  // 75th
    stats.flowPercentiles[4] = sorted[89];  // 90th
  }
  
  // Calculate usage trend (7-day moving average)
  static float dailyTotals[7] = {0};
  static int dayIndex = 0;
  static unsigned long lastDayUpdate = 0;
  
  if (millis() - lastDayUpdate > 86400000) {
    dailyTotals[dayIndex] = flow.dailyLitres;
    dayIndex = (dayIndex + 1) % 7;
    
    float sum = 0;
    for (int i = 0; i < 7; i++) sum += dailyTotals[i];
    stats.weeklyAverage = sum / 7;
    
    // Calculate trend
    if (stats.dailyAverage > 0) {
      stats.usageTrend = ((stats.weeklyAverage - stats.dailyAverage) / stats.dailyAverage) * 100;
    }
    stats.dailyAverage = flow.dailyLitres;
    
    lastDayUpdate = millis();
  }
}

// ============ Advanced Alert Detection ============
void checkAlerts() {
  unsigned long now = millis();
  
  // 1. High flow alert
  if (flow.flowRate > thresholds.maxFlowThreshold) {
    addAlert("HighFlow", 
             String("High flow detected: ") + flow.flowRate + " L/min (Threshold: " + thresholds.maxFlowThreshold + " L/min)",
             3);
  }
  
  // 2. Leak detection
  static unsigned long leakStartTime = 0;
  static bool leakChecking = false;
  
  if (flow.flowRate > thresholds.leakThreshold && flow.flowRate < thresholds.maxFlowThreshold) {
    if (!leakChecking) {
      leakStartTime = now;
      leakChecking = true;
    } else if (now - leakStartTime > thresholds.anomalyWindow) {
      addAlert("Leak", 
               "Continuous flow detected for over " + String(thresholds.anomalyWindow / 60000) + " minutes. Possible leak.",
               4);
      if (config.autoValveShutdown) handleValve(false);
    }
  } else {
    leakChecking = false;
  }
  
  // 3. No usage alert
  static unsigned long lastUsageTime = 0;
  if (flow.flowRate > 0.1) {
    lastUsageTime = now;
  } else if (now - lastUsageTime > (thresholds.noUsageThreshold * 3600000)) {
    addAlert("NoUsage",
             "No water usage detected for " + String(thresholds.noUsageThreshold) + " hours",
             2);
  }
  
  // 4. Sudden drop alert
  static float lastFlowRate = 0;
  static unsigned long lastCheckTime = 0;
  
  if (now - lastCheckTime > 60000) {  // Check every minute
    if (lastFlowRate > 1.0 && flow.flowRate > 0.1) {
      float dropPercent = ((lastFlowRate - flow.flowRate) / lastFlowRate) * 100;
      if (dropPercent > thresholds.suddenDropThreshold) {
        addAlert("SuddenDrop",
                 "Flow dropped by " + String(dropPercent) + "% in one minute",
                 3);
      }
    }
    lastFlowRate = flow.flowRate;
    lastCheckTime = now;
  }
  
  // 5. Water quality alerts
  if (waterQuality.temperature > thresholds.temperatureThreshold) {
    addAlert("HighTemp",
             "Water temperature high: " + String(waterQuality.temperature) + "°F",
             3);
  }
  
  if (waterQuality.pH < 6.5 || waterQuality.pH > 8.5) {
    addAlert("pHAlert",
             "Water pH out of range: " + String(waterQuality.pH),
             3);
  }
  
  // 6. Budget alert
  if (billing.dailyCost > (billing.budget / 30)) {
    addAlert("Budget",
             "Daily cost (" + String(billing.dailyCost) + ") exceeds budget limit",
             2);
    billing.budgetAlert = true;
  }
  
  // 7. System health alerts
  if (health.memoryUsage < 20) {  // Less than 20KB free
    addAlert("Memory",
             "Low memory: " + String(health.memoryUsage) + "KB free",
             3);
  }
  
  if (health.wifiSignal < -80) {  // Weak signal
    addAlert("WiFi",
             "Weak WiFi signal: " + String(health.wifiSignal) + " dBm",
             2);
  }
}

// ============ Advanced Predictions ============
void updatePredictions() {
  // Simple moving average prediction
  static float last7Days[7] = {0};
  static int dayPtr = 0;
  static unsigned long lastPrediction = 0;
  
  if (millis() - lastPrediction > 3600000) {  // Update hourly
    last7Days[dayPtr] = flow.dailyLitres;
    dayPtr = (dayPtr + 1) % 7;
    
    // Calculate weighted average
    float weightedSum = 0;
    float weightSum = 0;
    
    for (int i = 0; i < 7; i++) {
      int idx = (dayPtr - i + 7) % 7;
      weightedSum += last7Days[idx] * predictions.weights[i];
      weightSum += predictions.weights[i];
    }
    
    predictions.predictedDailyUsage = weightedSum / weightSum;
    predictions.predictedWeeklyUsage = predictions.predictedDailyUsage * 7;
    predictions.predictedMonthlyUsage = predictions.predictedDailyUsage * 30;
    
    // Adjust for season (simple simulation)
    if (timeInfo.timeSynced) {
      int month = timeInfo.timeinfo.tm_mon;
      // Higher usage in summer months (June-August in northern hemisphere)
      if (month >= 5 && month <= 7) {
        predictions.seasonalFactor = 1.2;
      } else if (month >= 11 || month <= 1) {  // Winter
        predictions.seasonalFactor = 0.8;
      } else {
        predictions.seasonalFactor = 1.0;
      }
      
      predictions.predictedDailyUsage *= predictions.seasonalFactor;
    }
    
    // Calculate confidence score based on data variance
    float variance = 0;
    float mean = 0;
    for (int i = 0; i < 7; i++) mean += last7Days[i];
    mean /= 7;
    
    for (int i = 0; i < 7; i++) {
      variance += pow(last7Days[i] - mean, 2);
    }
    variance /= 7;
    
    predictions.confidenceScore = 100 - (sqrt(variance) / mean * 100);
    predictions.confidenceScore = constrain(predictions.confidenceScore, 0, 100);
    
    lastPrediction = millis();
  }
}

// ============ Water Quality Monitoring ============
void updateWaterQuality() {
  // Simulate sensor readings - in real implementation, read from actual sensors
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate > 30000) {  // Update every 30 seconds
    // Simulate temperature (connected to actual sensor)
    int tempReading = analogRead(TEMP_SENSOR_PIN);
    waterQuality.temperature = (tempReading / 4095.0) * 100;  // Convert to °C
    waterQuality.temperature = waterQuality.temperature * 9/5 + 32;  // Convert to °F
    
    // Simulate other parameters
    waterQuality.pH = 7.0 + random(-5, 5) * 0.1;  // Small random variation
    waterQuality.turbidity = random(0, 10);  // NTU
    waterQuality.conductivity = random(50, 500);  // μS/cm
    waterQuality.chlorine = random(0, 5) * 0.1;  // mg/L
    
    // Check quality thresholds
    waterQuality.qualityAlert = (waterQuality.pH < 6.5 || waterQuality.pH > 8.5 ||
                                waterQuality.turbidity > 5.0 ||
                                waterQuality.chlorine > 4.0);
    
    lastUpdate = millis();
  }
}

// ============ Billing and Cost Calculation ============
void updateBilling() {
  static unsigned long lastBillingUpdate = 0;
  
  if (millis() - lastBillingUpdate > 60000) {  // Update every minute
    // Calculate costs
    billing.dailyCost = (flow.dailyLitres / 1000.0) * billing.ratePer1000L;
    billing.monthlyCost = (flow.monthlyLitres / 1000.0) * billing.ratePer1000L;
    billing.yearlyCost = (flow.yearlyLitres / 1000.0) * billing.ratePer1000L;
    billing.currentCost = (flow.totalLitres / 1000.0) * billing.ratePer1000L;
    
    // Calculate carbon footprint (approx 0.344 kg CO2 per 1000L of water)
    billing.carbonFootprint = flow.totalLitres * 0.344 / 1000.0;
    
    // Check budget
    billing.budgetAlert = billing.dailyCost > (billing.budget / 30);
    
    lastBillingUpdate = millis();
  }
}

// ============ Task Scheduler ============
void addTask(String name, unsigned long interval, void (*function)(), bool enabled = true) {
  if (taskCount < MAX_TASKS) {
    tasks[taskCount].name = name;
    tasks[taskCount].interval = interval;
    tasks[taskCount].lastRun = 0;
    tasks[taskCount].enabled = enabled;
    tasks[taskCount].function = function;
    taskCount++;
  }
}

void runScheduledTasks() {
  unsigned long now = millis();
  
  for (int i = 0; i < taskCount; i++) {
    if (tasks[i].enabled && (now - tasks[i].lastRun >= tasks[i].interval)) {
      tasks[i].function();
      tasks[i].lastRun = now;
    }
  }
}

// ============ Web Server Handlers ============
void handleRoot() {
  String html = R"rawliteral(

    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Hydro-Hero Dashboard</title>
      <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
      <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: #333; }
        .container { max-width: 1400px; margin: 0 auto; padding: 20px; }
        header { background: rgba(255, 255, 255, 0.95); padding: 20px; border-radius: 15px; margin-bottom: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.1); }
        .dashboard { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: white; padding: 25px; border-radius: 15px; box-shadow: 0 5px 15px rgba(0,0,0,0.08); }
        .stat { font-size: 2.5em; font-weight: bold; color: #4a5568; margin: 10px 0; }
        .label { color: #718096; font-size: 0.9em; text-transform: uppercase; letter-spacing: 1px; }
        .alert { background: #fed7d7; border-left: 4px solid #c53030; }
        .good { background: #c6f6d5; border-left: 4px solid #38a169; }
        .nav { display: flex; gap: 15px; margin-bottom: 20px; flex-wrap: wrap; }
        .nav a { background: white; padding: 12px 25px; text-decoration: none; color: #4a5568; border-radius: 8px; transition: all 0.3s; }
        .nav a:hover { background: #667eea; color: white; transform: translateY(-2px); }
        canvas { width: 100% !important; height: 300px !important; }
        .btn { background: #667eea; color: white; border: none; padding: 12px 24px; border-radius: 8px; cursor: pointer; transition: all 0.3s; }
        .btn:hover { background: #5a67d8; transform: translateY(-2px); }
      </style>
    </head>
    <body>
      <div class="container">
        <header>
          <h1>🚰 Hydro-Hero Dashboard</h1>
          <p>Device: %DEVICE_NAME% | Location: %LOCATION% | Uptime: %UPTIME%</p>
        </header>
        
        <div class="nav">
          <a href="/">Dashboard</a>
          <a href="/analytics">Analytics</a>
          <a href="/alerts">Alerts</a>
          <a href="/zones">Zones</a>
          <a href="/quality">Water Quality</a>
          <a href="/billing">Billing</a>
          <a href="/system">System</a>
          <a href="/settings">Settings</a>
          <a href="/api">API</a>
        </div>
        
        <div class="dashboard">
          <!-- Flow Information -->
          <div class="card">
            <h3>Flow Information</h3>
            <div class="stat">%FLOW_RATE% L/min</div>
            <div class="label">Current Flow Rate</div>
            <div class="stat">%TOTAL_LITRES% L</div>
            <div class="label">Total Consumption</div>
            <div class="stat">%DAILY_LITRES% L</div>
            <div class="label">Today's Usage</div>
          </div>
          
          <!-- System Status -->
          <div class="card %SYSTEM_STATUS%">
            <h3>System Status</h3>
            <p>WiFi: %WIFI_STRENGTH% dBm</p>
            <p>Memory: %FREE_MEM% KB free</p>
            <p>Valve: %VALVE_STATUS%</p>
            <p>Last Alert: %LAST_ALERT%</p>
          </div>
          
          <!-- Alerts Summary -->
          <div class="card">
            <h3>Active Alerts</h3>
            %ALERTS_SUMMARY%
          </div>
          
          <!-- Water Quality -->
          <div class="card">
            <h3>Water Quality</h3>
            <p>Temperature: %WATER_TEMP%°F</p>
            <p>pH: %WATER_PH%</p>
            <p>Turbidity: %WATER_TURBIDITY% NTU</p>
            <p>Status: %QUALITY_STATUS%</p>
          </div>
          
          <!-- Charts -->
          <div class="card" style="grid-column: span 2;">
            <h3>Flow History (Last Hour)</h3>
            <canvas id="flowChart"></canvas>
          </div>
          
          <!-- Quick Actions -->
          <div class="card">
            <h3>Quick Actions</h3>
            <button class="btn" onclick="toggleValve()">Toggle Valve</button>
            <button class="btn" onclick="acknowledgeAlerts()">Acknowledge Alerts</button>
            <button class="btn" onclick="generateReport()">Generate Report</button>
            <button class="btn" onclick="rebootSystem()">Reboot System</button>
          </div>
        </div>
      </div>
      
      <script>
        // Update data every 5 seconds
        setInterval(updateDashboard, 5000);
        
        async function updateDashboard() {
          const response = await fetch('/api/status');
          const data = await response.json();
          
          // Update all data fields
          document.querySelectorAll('[data-field]').forEach(el => {
            const field = el.getAttribute('data-field');
            if (data[field] !== undefined) {
              el.textContent = data[field];
            }
          });
        }
        
        function toggleValve() {
          fetch('/api/valve/toggle', { method: 'POST' });
        }
        
        // Initialize chart
        const ctx = document.getElementById('flowChart').getContext('2d');
        const flowChart = new Chart(ctx, {
          type: 'line',
          data: {
            labels: [],
            datasets: [{
              label: 'Flow Rate (L/min)',
              data: [],
              borderColor: 'rgb(75, 192, 192)',
              tension: 0.1
            }]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false
          }
        });
      </script>
    </body>
    </html>
  )rawliteral";

  
  // Replace placeholders with actual data
  html.replace("%DEVICE_NAME%", device.name);
  html.replace("%LOCATION%", device.location);
  html.replace("%UPTIME%", String(health.uptime / 3600) + " hours");
  html.replace("%FLOW_RATE%", String(flow.flowRate, 2));
  html.replace("%TOTAL_LITRES%", String(flow.totalLitres, 1));
  html.replace("%DAILY_LITRES%", String(flow.dailyLitres, 1));
  html.replace("%SYSTEM_STATUS%", (health.memoryUsage > 50 && health.wifiSignal > -70) ? "good" : "alert");
  html.replace("%WIFI_STRENGTH%", String(health.wifiSignal));
  html.replace("%FREE_MEM%", String(health.memoryUsage));
  html.replace("%VALVE_STATUS%", digitalRead(VALVE_RELAY_PIN) ? "Open" : "Closed");
  html.replace("%LAST_ALERT%", alerts[(alertIndex - 1 + MAX_ALERTS) % MAX_ALERTS].message.substring(0, 30));
  html.replace("%WATER_TEMP%", String(waterQuality.temperature, 1));
  html.replace("%WATER_PH%", String(waterQuality.pH, 1));
  html.replace("%WATER_TURBIDITY%", String(waterQuality.turbidity, 1));
  html.replace("%QUALITY_STATUS%", waterQuality.qualityAlert ? "⚠️ Alert" : "✅ Good");
  
  // Build alerts summary
  String alertsSummary = "";
  int activeCount = 0;
  for (int i = 0; i < MAX_ALERTS; i++) {
    if (alerts[i].active && !alerts[i].acknowledged) {
      alertsSummary += "<p>⚠️ " + alerts[i].message.substring(0, 40) + "...</p>";
      activeCount++;
    }
  }
  if (activeCount == 0) alertsSummary = "<p>✅ No active alerts</p>";
  html.replace("%ALERTS_SUMMARY%", alertsSummary);
  
  server.send(200, "text/html", html);
}

void handleAPIStatus() {
  DynamicJsonDocument doc(2048);
  
  doc["device"]["name"] = device.name;
  doc["device"]["id"] = device.deviceID;
  doc["device"]["version"] = device.firmwareVersion;
  doc["device"]["location"] = device.location;
  doc["device"]["uptime"] = health.uptime;
  
  doc["flow"]["rate"] = flow.flowRate;
  doc["flow"]["instantaneous"] = flow.instantaneousFlow;
  doc["flow"]["total"] = flow.totalLitres;
  doc["flow"]["daily"] = flow.dailyLitres;
  doc["flow"]["weekly"] = flow.weeklyLitres;
  doc["flow"]["monthly"] = flow.monthlyLitres;
  doc["flow"]["yearly"] = flow.yearlyLitres;
  
  doc["stats"]["average"] = stats.averageFlow;
  doc["stats"]["peak"] = stats.peakFlow;
  doc["stats"]["min"] = stats.minFlow;
  doc["stats"]["median"] = stats.medianFlow;
  doc["stats"]["trend"] = stats.usageTrend;
  
  doc["quality"]["temperature"] = waterQuality.temperature;
  doc["quality"]["ph"] = waterQuality.pH;
  doc["quality"]["turbidity"] = waterQuality.turbidity;
  doc["quality"]["conductivity"] = waterQuality.conductivity;
  doc["quality"]["chlorine"] = waterQuality.chlorine;
  doc["quality"]["alert"] = waterQuality.qualityAlert;
  
  doc["billing"]["dailyCost"] = billing.dailyCost;
  doc["billing"]["monthlyCost"] = billing.monthlyCost;
  doc["billing"]["budget"] = billing.budget;
  doc["billing"]["budgetAlert"] = billing.budgetAlert;
  doc["billing"]["carbonFootprint"] = billing.carbonFootprint;
  
  doc["system"]["memory"] = health.memoryUsage;
  doc["system"]["wifi"] = health.wifiSignal;
  doc["system"]["voltage"] = health.voltage;
  doc["system"]["temperature"] = health.temperature;
  doc["system"]["cpuLoad"] = health.cpuLoad;
  
  // Count active alerts
  int activeAlerts = 0;
  for (int i = 0; i < MAX_ALERTS; i++) {
    if (alerts[i].active && !alerts[i].acknowledged) activeAlerts++;
  }
  doc["alerts"]["active"] = activeAlerts;
  doc["alerts"]["total"] = alertIndex;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ============ Additional Handlers ============
void handleAnalytics() {
  server.send(200, "text/html", "<h1>Analytics Page</h1><p>Detailed analytics coming soon...</p>");
}

void handleAlertsPage() {
  String html = "<h1>Alerts</h1>";
  for (int i = 0; i < MAX_ALERTS; i++) {
    if (alerts[i].active) {
      html += "<div style='border:1px solid #ccc;margin:5px;padding:10px;'>";
      html += "<strong>" + alerts[i].type + "</strong>: " + alerts[i].message;
      html += "<br><small>Severity: " + String(alerts[i].severity) + "</small>";
      html += "<button onclick='acknowledge(" + String(i) + ")'>Acknowledge</button>";
      html += "</div>";
    }
  }
  server.send(200, "text/html", html);
}

// ============ Hardware Control ============
void handleValve(bool state) {
  digitalWrite(VALVE_RELAY_PIN, state ? HIGH : LOW);
  logEvent("Valve", state ? "Valve opened" : "Valve closed", 2, "Control");
  addAlert("Valve", state ? "Manual valve open" : "Emergency valve closure", 2);
}

void playAlertTone(int pattern) {
  if (!config.soundAlerts) return;
  
  switch(pattern) {
    case 1: // Low priority
      tone(BUZZER_PIN, 1000, 200);
      break;
    case 2: // Medium priority
      tone(BUZZER_PIN, 1500, 200);
      delay(300);
      tone(BUZZER_PIN, 1500, 200);
      break;
    case 3: // High priority
      for(int i = 0; i < 3; i++) {
        tone(BUZZER_PIN, 2000, 200);
        delay(200);
      }
      break;
    case 4: // Critical
      for(int i = 0; i < 5; i++) {
        tone(BUZZER_PIN, 2500, 100);
        delay(100);
      }
      break;
  }
}

void updateLEDStatus() {
  if (!config.ledIndicators) {
    digitalWrite(STATUS_LED_PIN, LOW);
    return;
  }
  
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  // Determine blink pattern based on status
  int blinkPattern = 0;
  
  // Check for critical alerts first
  bool criticalAlert = false;
  for (int i = 0; i < MAX_ALERTS; i++) {
    if (alerts[i].active && !alerts[i].acknowledged && alerts[i].severity == 4) {
      criticalAlert = true;
      break;
    }
  }
  
  if (criticalAlert) {
    blinkPattern = 100;  // Fast blink
  } else if (flow.flowRate > thresholds.maxFlowThreshold) {
    blinkPattern = 500;  // Medium blink
} else if (flow.flowRate > thresholds.leakThreshold) {
  blinkPattern = 1000; // Slow blink
}
  } else {
    // Normal operation - heartbeat
    static unsigned long heartbeat = 0;
    if (millis() - heartbeat > 2000) {
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(50);
      digitalWrite(STATUS_LED_PIN, LOW);
      heartbeat = millis();
    }
    return;
  }
  
  // Implement blinking
  if (millis() - lastBlink > blinkPattern) {
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
    lastBlink = millis();
  }
}

// ============ System Health Monitoring ============
void checkSystemHealth() {
  // Update WiFi signal strength
  health.wifiSignal = WiFi.RSSI();
  
  // Update free memory
  health.memoryUsage = ESP.getFreeHeap() / 1024.0;
  
  // Update uptime
  health.uptime = millis() / 1000.0;
  
  // Simulate temperature reading
  health.temperature = 25.0 + random(-5, 5);
  
  // Check for system issues
  if (health.memoryUsage < 20) {
    logEvent("System", "Low memory warning", 3, "Health");
  }
  
  if (health.wifiSignal < -85) {
    logEvent("System", "Weak WiFi signal", 2, "Health");
  }
  
  // Update CPU load simulation
  static unsigned long lastIdleTime = 0;
  unsigned long busyTime = millis() - lastIdleTime;
  health.cpuLoad = min(100.0, (busyTime / 100.0));
  lastIdleTime = millis();
}

// ============ Data History Management ============
void saveToHistory() {
  flow.flowHistory[flow.historyIndex] = flow.flowRate;
  flow.pressureHistory[flow.historyIndex] = 50 + random(-10, 10);  // Simulated pressure
  flow.historyTimestamps[flow.historyIndex] = millis();
  
  flow.historyIndex = (flow.historyIndex + 1) % HISTORY_SIZE;
}

// ============ Time Synchronization ============
void syncTime() {
  if (!timeInfo.timeSynced || (millis() - timeInfo.lastSync > timeInfo.syncInterval)) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    
    if (getLocalTime(&timeInfo.timeinfo)) {
      timeInfo.timeSynced = true;
      timeInfo.lastSync = millis();
      logEvent("System", "Time synchronized: " + String(asctime(&timeInfo.timeinfo)), 2, "Time");
    } else {
      logEvent("System", "Time synchronization failed", 3, "Time");
    }
  }
}

// ============ Notification System ============
void sendNotification(String message) {
  // Placeholder for notification system
  // In real implementation, integrate with:
  // - Email (SMTP)
  // - SMS (Twilio, etc.)
  // - Push notifications
  // - Webhooks
  // - MQTT
  
  logEvent("Notification", message, 2, "Comms");
}

// ============ Zone Management ============
void initializeZones() {
  // Initialize default zones
  zones[0] = {"Main Supply", FLOW_SENSOR_PIN, true, 0, 0, 0, false, 0, 10.0};
  zones[1] = {"Kitchen", -1, true, 0, 0, 0, false, 0, 5.0};
  zones[2] = {"Bathroom", -1, true, 0, 0, 0, false, 0, 3.0};
  zones[3] = {"Garden", -1, true, 0, 0, 0, false, 0, 8.0};
  zones[4] = {"Laundry", -1, true, 0, 0, 0, false, 0, 4.0};
  
  zoneCount = 5;
}

void updateZoneData() {
  // Simulate zone data - in real implementation, read from zone sensors
  for (int i = 0; i < zoneCount; i++) {
    if (zones[i].enabled && zones[i].sensorPin == -1) {
      // Simulate random zone usage
      zones[i].flowRate = random(0, 100) / 10.0;
      zones[i].dailyUsage += zones[i].flowRate / 60.0;  // Convert L/min to L
      
      // Check zone-specific leaks
      if (zones[i].flowRate > 0.1) {
        if (zones[i].leakStartTime == 0) {
          zones[i].leakStartTime = millis();
        } else if (millis() - zones[i].leakStartTime > 300000) {  // 5 minutes
          zones[i].leakDetected = true;
          addAlert("ZoneLeak", "Possible leak in " + zones[i].name + " zone", 3);
        }
      } else {
        zones[i].leakStartTime = 0;
        zones[i].leakDetected = false;
      }
    }
  }
}

// ============ Setup Function ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n🚰 Hydro-Hero Water Monitoring System");
  Serial.println("===================================");
  Serial.println("Firmware Version: " + device.firmwareVersion);
  
  // Initialize pins
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(VALVE_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(TEMP_SENSOR_PIN, INPUT);
  
  // Initialize valve as closed (safe state)
  digitalWrite(VALVE_RELAY_PIN, LOW);
  
  // Attach flow sensor interrupt
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
  
  // Initialize EEPROM and load configuration
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
  }
  loadConfig();
  
  // Initialize SPIFFS for data logging
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed");
  }
  
  // Initialize WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    logEvent("Network", "WiFi connected: " + WiFi.localIP().toString(), 2, "Network");
  } else {
    Serial.println("\nWiFi connection failed");
    logEvent("Network", "WiFi connection failed", 3, "Network");
  }
  
  // Initialize web server
  server.on("/", handleRoot);
  server.on("/api/status", handleAPIStatus);
  server.on("/api/valve/toggle", []() {
    bool currentState = digitalRead(VALVE_RELAY_PIN);
    handleValve(!currentState);
    server.send(200, "text/plain", currentState ? "Valve closed" : "Valve opened");
  });
  server.on("/analytics", handleAnalytics);
  server.on("/alerts", handleAlertsPage);
  server.on("/api/alerts/acknowledge", []() {
    for (int i = 0; i < MAX_ALERTS; i++) {
      alerts[i].acknowledged = true;
    }
    server.send(200, "text/plain", "All alerts acknowledged");
  });
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize zones
  initializeZones();
  
  // Schedule tasks
  addTask("UpdateFlow", UPDATE_INTERVAL, updateFlowData);
  addTask("CheckAlerts", 5000, checkAlerts);
  addTask("UpdateStats", 30000, updateStatistics);
  addTask("UpdatePredictions", 60000, updatePredictions);
  addTask("UpdateWaterQuality", 30000, updateWaterQuality);
  addTask("UpdateBilling", 60000, updateBilling);
  addTask("CheckSystemHealth", 10000, checkSystemHealth);
  addTask("UpdateLED", 100, updateLEDStatus);
  addTask("SaveHistory", 10000, saveToHistory);
  addTask("SyncTime", 3600000, syncTime);
  addTask("UpdateZones", 15000, updateZoneData);
  addTask("BackupData", 3600000, backupData);
  
  // Initial time sync attempt
  syncTime();
  
  // Welcome message
  logEvent("System", "Hydro-Hero initialized successfully", 1, "System");
  
  // Initial beep
  if (config.soundAlerts) {
    tone(BUZZER_PIN, 2000, 100);
    delay(150);
    tone(BUZZER_PIN, 2500, 100);
  }
}

// ============ Flow Interrupt Handler ============
void IRAM_ATTR pulseCounter() {
  flow.pulseCount++;
}

// ============ Main Flow Update Function ============
void updateFlowData() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  unsigned long deltaTime = now - lastUpdate;
  
  if (deltaTime >= UPDATE_INTERVAL) {
    // Calculate flow
    detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));
    
    float litres = flow.pulseCount / flow.pulsesPerLitre;
    flow.instantaneousFlow = litres / (deltaTime / 1000.0);  // L/s
    flow.flowRate = flow.instantaneousFlow * 60.0;           // L/min
    
    // Update totals
    flow.totalLitres += litres;
    flow.dailyLitres += litres;
    flow.weeklyLitres += litres;
    flow.monthlyLitres += litres;
    flow.yearlyLitres += litres;
    
    // Reset pulse count
    flow.pulseCount = 0;
    
    // Reattach interrupt
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
    
    lastUpdate = now;
  }
}

// ============ Data Backup ============
void backupData() {
  // Placeholder for data backup functionality
  // Could save to SPIFFS, SD card, or cloud
  logEvent("System", "Data backup completed", 2, "Storage");
}

// ============ Main Loop ============
void loop() {
  // Handle web server clients
  server.handleClient();
  
  // Run scheduled tasks
  runScheduledTasks();
  
  // Daily reset at midnight
  static unsigned long lastDayReset = 0;
  if (millis() - lastDayReset > 86400000) {  // 24 hours
    flow.dailyLitres = 0;
    billing.dailyCost = 0;
    lastDayReset = millis();
    
    // Weekly reset (every 7 days)
    static int dayCount = 0;
    dayCount++;
    if (dayCount >= 7) {
      flow.weeklyLitres = 0;
      dayCount = 0;
    }
    
    // Monthly reset (approximate - 30 days)
    static int monthDayCount = 0;
    monthDayCount++;
    if (monthDayCount >= 30) {
      flow.monthlyLitres = 0;
      billing.monthlyCost = 0;
      monthDayCount = 0;
    }
    
    logEvent("System", "Daily reset performed", 1, "System");
  }
  
  // Manual reset check (for development)
  static unsigned long lastManualReset = 0;
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "reset") {
      flow.dailyLitres = 0;
      flow.weeklyLitres = 0;
      flow.monthlyLitres = 0;
      Serial.println("All counters reset");
      logEvent("System", "Manual reset via serial", 2, "System");
    } else if (cmd == "valve on") {
      handleValve(true);
    } else if (cmd == "valve off") {
      handleValve(false);
    } else if (cmd == "status") {
      Serial.println("Flow Rate: " + String(flow.flowRate) + " L/min");
      Serial.println("Total: " + String(flow.totalLitres) + " L");
      Serial.println("Daily: " + String(flow.dailyLitres) + " L");
    }
  }
  
  // Small delay to prevent watchdog timer issues
  delay(10);
}