#include <Arduino.h>
#include "AppContext.h"
#include "CommandSubmitter.h"
#include "LogBuffer.h"
#include "PlotterConfig.h"

#if ESP32_WEBUI_ENABLED
#include <WebServer.h>
#include <WiFi.h>

namespace {
WebServer server(ESP32_WEBUI_PORT);
bool http_job_stream_active = false;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'><title>CoreXY Plotter ESP32 WebUI</title><style>
body{margin:0;background:#0b1220;color:#e5eefb;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif}main{max-width:760px;margin:auto;padding:14px}.card{background:#151f32;border:1px solid #2a3854;border-radius:14px;padding:14px;margin:12px 0}.row{display:flex;gap:8px;flex-wrap:wrap}.metric{flex:1;min-width:92px;background:#0f1728;border-radius:10px;padding:10px}.label{font-size:12px;color:#95a3b8}.value{font-size:22px;font-weight:700}button{border:0;border-radius:10px;padding:12px 14px;background:#2563eb;color:white;font-weight:700}button.warn{background:#dc2626}button.secondary{background:#334155}button:disabled{background:#475569;color:#94a3b8}input,textarea,select{width:100%;box-sizing:border-box;border-radius:10px;border:1px solid #334155;background:#0f1728;color:#e5eefb;padding:10px}textarea{min-height:150px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}.console{height:170px;overflow:auto;background:#050814;border-radius:10px;padding:10px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:12px;white-space:pre-wrap}.ok{color:#22c55e}.bad{color:#f97316}</style></head><body><main><h2>CoreXY Plotter ESP32 WebUI</h2>
<section class='card'><div class='row'><div class='metric'><div class='label'>State</div><div id='state' class='value'>...</div></div><div class='metric'><div class='label'>X</div><div id='x' class='value'>0.0</div></div><div class='metric'><div class='label'>Y</div><div id='y' class='value'>0.0</div></div><div class='metric'><div class='label'>Pen</div><div id='pen' class='value'>...</div></div></div><p id='detail' class='label'></p></section>
<section class='card'><h3>Manual</h3><div class='row'><button onclick="cmd('HOME')">HOME</button><button onclick="cmd('POS')">POS</button><button onclick="cmd('ALARM_CLEAR')">ALARM CLEAR</button><button onclick="cmd('M5')">PEN UP</button><button onclick="cmd('M3')">PEN DOWN</button><button class='warn' onclick='abortJob()'>ABORT</button></div><h4>Jog</h4><div class='row'><label>Step mm<select id='step'><option>0.1</option><option selected>1</option><option>5</option><option>10</option></select></label><label>Feed mm/min<input id='feed' type='number' value='900'></label></div><div class='row' style='margin-top:8px'><button onclick="jog(0,1)">Y+</button><button onclick="jog(-1,0)">X-</button><button onclick="jog(1,0)">X+</button><button onclick="jog(0,-1)">Y-</button></div></section>
<section class='card'><h3>G-code Job</h3><input id='file' type='file' accept='.gcode,.nc,.txt'><p><textarea id='gcode' placeholder='Paste G-code here'></textarea></p><div class='row'><button id='send' onclick='sendJob()'>Send Job</button><button class='warn' onclick='abortJob()'>Abort</button></div><p id='progress' class='label'>Idle</p></section>
<section class='card'><h3>Console</h3><div id='console' class='console'></div></section></main><script>
let statusData=null,sending=false;const sleep=ms=>new Promise(r=>setTimeout(r,ms));function log(t,bad=false){const c=document.getElementById('console');const s=document.createElement('div');s.className=bad?'bad':'ok';s.textContent=new Date().toLocaleTimeString()+' '+t;c.appendChild(s);c.scrollTop=c.scrollHeight}async function api(path,body){const opt=body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{};const r=await fetch(path,opt);const j=await r.json().catch(()=>({ok:false,reason:'bad_json'}));if(!r.ok)throw j;return j}async function refresh(){try{const j=await api('/api/status');statusData=j.machine;state.textContent=j.machine.state;x.textContent=j.machine.x.toFixed(1);y.textContent=j.machine.y.toFixed(1);pen.textContent=j.machine.pen;detail.textContent='homed='+j.machine.homed+' alarm='+j.machine.alarmed+' job='+j.machine.jobState+' queue='+j.machine.queueDepth+' ip='+j.ip}catch(e){log('status failed '+(e.reason||e.error||e),true)}}async function refreshLogs(){try{const j=await api('/api/logs');console.textContent=j.logs.join('\n');console.scrollTop=console.scrollHeight}catch(e){}}async function cmd(command){try{const j=await api('/api/command',{command});log(command+' '+j.reason);await refresh()}catch(e){log(command+' '+(e.reason||e.error||e),true)}}async function jog(dx,dy){if(!statusData){await refresh()}const s=parseFloat(step.value),f=parseFloat(feed.value||'900');const nx=(statusData.x+dx*s).toFixed(3),ny=(statusData.y+dy*s).toFixed(3);await cmd('G1 X'+nx+' Y'+ny+' F'+f)}file.addEventListener('change',async e=>{const f=e.target.files[0];if(f)gcode.value=await f.text()});async function postLine(line){for(let i=0;i<80;i++){try{return await api('/api/job/line',{line})}catch(e){if(e.reason==='queue_full'){await sleep(e.retryAfterMs||150);continue}throw e}}throw {reason:'queue_full_timeout'}}async function sendJob(){if(sending)return;const text=gcode.value;if(!text.trim()){log('no gcode',true);return}sending=true;send.disabled=true;const lines=text.split(/\r?\n/);try{await api('/api/job/begin',{name:'iphone_upload.gcode'});for(let i=0;i<lines.length;i++){await postLine(lines[i]);if(i%5===0){progress.textContent='Sending '+(i+1)+' / '+lines.length;await sleep(1)}}await api('/api/job/end',{});progress.textContent='Queued '+lines.length+' lines';log('job queued')}catch(e){progress.textContent='Error';log('job failed '+(e.reason||e.error||e),true)}finally{sending=false;send.disabled=false;await refresh()}}async function abortJob(){try{await api('/api/job/abort',{});log('abort requested')}catch(e){log('abort failed '+(e.reason||e.error||e),true)}await refresh()}setInterval(refresh,1000);setInterval(refreshLogs,2500);refresh();refreshLogs();
</script></body></html>
)HTML";

String jsonEscape(const char* text) {
  String escaped;
  if (text == nullptr) return escaped;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    const char c = *cursor;
    if (c == '"') escaped += "\\\"";
    else if (c == '\\') escaped += "\\\\";
    else if (c == '\n') escaped += "\\n";
    else if (c == '\r') escaped += "\\r";
    else escaped += c;
  }
  return escaped;
}

uint16_t commandQueueDepth() {
  return command_queue == nullptr ? 0 : uxQueueMessagesWaiting(command_queue);
}

const char* stateText() {
  if (machine_state.alarmed || safety_manager.isAlarmed()) return "ALARM";
  if (machine_state.homing_active) return "HOMING";
  if (machine_state.motion_active) return "MOVING";
  if (job_controller.isActive() || job_controller.isRunning()) return "RUNNING";
  if (!machine_state.homed) return "NEED HOME";
  return "READY";
}

void sendJson(int code, const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", json);
}

void sendError(int code, const char* reason) {
  String json = "{\"ok\":false,\"reason\":\"";
  json += jsonEscape(reason);
  json += "\",\"queueDepth\":";
  json += commandQueueDepth();
  json += "}";
  sendJson(code, json);
}

bool extractJsonString(const String& body, const char* key, char* out, size_t out_size) {
  if (out_size == 0) return false;
  out[0] = '\0';
  const String needle = String("\"") + key + "\"";
  int index = body.indexOf(needle);
  if (index < 0) return false;
  index = body.indexOf(':', index + needle.length());
  if (index < 0) return false;
  index = body.indexOf('"', index + 1);
  if (index < 0) return false;
  ++index;
  size_t write = 0;
  bool escape = false;
  for (; index < body.length(); ++index) {
    const char c = body[index];
    if (escape) {
      char decoded = c;
      if (c == 'n') decoded = '\n';
      else if (c == 'r') decoded = '\r';
      else if (c == 't') decoded = '\t';
      if (write + 1 >= out_size) return false;
      out[write++] = decoded;
      escape = false;
    } else if (c == '\\') {
      escape = true;
    } else if (c == '"') {
      out[write] = '\0';
      return true;
    } else {
      if (write + 1 >= out_size) return false;
      out[write++] = c;
    }
  }
  return false;
}

bool readJsonCommand(const char* key, char* out, size_t out_size) {
  const String body = server.arg("plain");
  if (extractJsonString(body, key, out, out_size)) return true;
  if (!body.startsWith("{") && body.length() + 1 < out_size) {
    snprintf(out, out_size, "%s", body.c_str());
    return true;
  }
  return false;
}

void sendSubmitResult(const CommandSubmitResult& result) {
  const bool accepted = commandSubmitAccepted(result.code);
  int status = accepted ? 200 : 400;
  if (result.code == CommandSubmitCode::QUEUE_FULL) status = 429;
  String json = "{\"ok\":";
  json += accepted ? "true" : "false";
  json += ",\"accepted\":";
  json += accepted ? "true" : "false";
  json += ",\"reason\":\"";
  json += result.reason[0] != '\0' ? jsonEscape(result.reason) : commandSubmitCodeName(result.code);
  json += "\",\"command\":\"";
  json += jsonEscape(result.command);
  json += "\",\"queueDepth\":";
  json += result.queue_depth;
  if (result.code == CommandSubmitCode::QUEUE_FULL) {
    json += ",\"retryAfterMs\":";
    json += ESP32_WEBUI_QUEUE_RETRY_AFTER_MS;
  }
  json += "}";
  sendJson(status, json);
}

bool isSkippableGcodeLine(const char* line) {
  if (line == nullptr) return true;
  while (*line == ' ' || *line == '\t') ++line;
  return *line == '\0' || *line == ';' || *line == '(' || *line == '%';
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  safety_manager.poll();
  machine_state.alarmed = safety_manager.isAlarmed();
  machine_state.job_active = job_controller.isActive() || job_controller.isRunning();
  String json = "{\"ok\":true,\"ip\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",\"machine\":{";
  json += "\"state\":\""; json += stateText(); json += "\",";
  json += "\"x\":"; json += String(machine_state.x_mm, 3); json += ',';
  json += "\"y\":"; json += String(machine_state.y_mm, 3); json += ',';
  json += "\"pen\":\""; json += machine_state.pen_down ? "DOWN" : "UP"; json += "\",";
  json += "\"homed\":"; json += machine_state.homed ? "true" : "false"; json += ',';
  json += "\"alarmed\":"; json += machine_state.alarmed ? "true" : "false"; json += ',';
  json += "\"alarmReason\":\""; json += jsonEscape(safety_manager.alarmReason()); json += "\",";
  json += "\"motion\":"; json += machine_state.motion_active ? "true" : "false"; json += ',';
  json += "\"homing\":"; json += machine_state.homing_active ? "true" : "false"; json += ',';
  json += "\"jobRunning\":"; json += machine_state.job_active ? "true" : "false"; json += ',';
  json += "\"jobState\":\""; json += job_controller.stateName(); json += "\",";
  json += "\"jobResult\":\""; json += jsonEscape(job_controller.result()); json += "\",";
  json += "\"jobLastError\":\""; json += jsonEscape(job_controller.lastError()); json += "\",";
  json += "\"queueDepth\":"; json += commandQueueDepth(); json += ',';
  json += "\"limits\":{";
  json += "\"x\":\""; json += safety_manager.xLimitActive() ? "ACTIVE" : "OPEN"; json += "\",";
  json += "\"y\":\""; json += safety_manager.yLimitActive() ? "ACTIVE" : "OPEN"; json += "\"}";
  json += "}}";
  sendJson(200, json);
}

void handleCommand() {
  char command[ESP32_WEBUI_MAX_COMMAND_LINE_LENGTH + 1] = {};
  if (!readJsonCommand("command", command, sizeof(command))) {
    sendError(400, "command_required_or_too_long");
    return;
  }
  sendSubmitResult(submitCommandLine(command, CommandSubmitMode::HTTP_NOWAIT));
}

void handleJobBegin() {
  if (http_job_stream_active) {
    sendError(409, "http_job_already_streaming");
    return;
  }
  CommandSubmitResult result = submitCommandLine("JOB_BEGIN", CommandSubmitMode::HTTP_NOWAIT);
  if (commandSubmitAccepted(result.code)) {
    http_job_stream_active = true;
  }
  sendSubmitResult(result);
}

void handleJobLine() {
  if (!http_job_stream_active) {
    sendError(409, "job_not_started");
    return;
  }
  char line[ESP32_WEBUI_MAX_GCODE_LINE_LENGTH + 1] = {};
  if (!readJsonCommand("line", line, sizeof(line))) {
    sendError(400, "line_required_or_too_long");
    return;
  }
  if (isSkippableGcodeLine(line)) {
    String json = "{\"ok\":true,\"accepted\":true,\"reason\":\"skipped\",\"queueDepth\":";
    json += commandQueueDepth();
    json += "}";
    sendJson(200, json);
    return;
  }
  CommandSubmitResult result = submitCommandLine(line, CommandSubmitMode::HTTP_NOWAIT, true);
  sendSubmitResult(result);
}

void handleJobEnd() {
  if (!http_job_stream_active) {
    sendError(409, "job_not_started");
    return;
  }
  CommandSubmitResult result = submitCommandLine("JOB_END", CommandSubmitMode::HTTP_NOWAIT);
  if (commandSubmitAccepted(result.code)) {
    http_job_stream_active = false;
  }
  sendSubmitResult(result);
}

void handleJobAbort() {
  http_job_stream_active = false;
  sendSubmitResult(submitCommandLine("JOB_ABORT", CommandSubmitMode::HTTP_NOWAIT));
}

void handleLogs() {
  String json = "{\"ok\":true,\"logs\":";
  json += latestLogBufferJson();
  json += "}";
  sendJson(200, json);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptions();
    return;
  }
  sendError(404, "not_found");
}

void beginWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ESP32_WEBUI_AP_SSID, ESP32_WEBUI_AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  logMessage("ESP32 WebUI AP SSID=%s IP=%s URL=http://%s/",
             ESP32_WEBUI_AP_SSID, ip.toString().c_str(), ip.toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/command", HTTP_POST, handleCommand);
  server.on("/api/job/begin", HTTP_POST, handleJobBegin);
  server.on("/api/job/line", HTTP_POST, handleJobLine);
  server.on("/api/job/end", HTTP_POST, handleJobEnd);
  server.on("/api/job/abort", HTTP_POST, handleJobAbort);
  server.on("/api/logs", HTTP_GET, handleLogs);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/command", HTTP_OPTIONS, handleOptions);
  server.on("/api/job/begin", HTTP_OPTIONS, handleOptions);
  server.on("/api/job/line", HTTP_OPTIONS, handleOptions);
  server.on("/api/job/end", HTTP_OPTIONS, handleOptions);
  server.on("/api/job/abort", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);
  server.begin();
  logMessage("ESP32 WebUI HTTP server started port=%u", ESP32_WEBUI_PORT);
}
}

void esp32WebTask(void*) {
  beginWebServer();
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

#else
void esp32WebTask(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
#endif
