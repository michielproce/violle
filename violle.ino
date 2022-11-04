/*
  5 rings, starting from the center.

  Ring 0:  2x2 =   4 LEDs (index:   0)
  Ring 1:  7x4 =  28 LEDs (index:   4)
  Ring 2: 13x4 =  52 LEDs (index:  32)
  Ring 3: 20x4 =  80 LEDs (index:  84)
  Ring 4: 25x4 = 100 LEDs (index: 164)

  Each ring starts top left, continues counterclockwise.
  Ring 0 start bottom left, by design of course.
*/

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define NUM_LEDS 264
#define DATA_PIN 13
#define TEMPERATURE_PIN 36

const char* ssid = "SSID";
const char* password = "PASSWORD";
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, user-scalable=no, initial-scale=1.0, maximum-scale=1.0, minimum-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
    <title>Violle</title>
</head>

<style>
    @import url('https://fonts.googleapis.com/css2?family=Merriweather&display=swap');
    @import url('https://fonts.googleapis.com/css2?family=Oswald&display=swap');

    * {
        box-sizing: border-box;
    }

    html, body, div, h1 {
        margin: 0;
        padding: 0;
        font-family: 'Oswald', sans-serif;
    }

    html, body {
        overflow: hidden;
        height: 100vh;
        margin: 0;
        padding: 0;
    }

    body {
        background: rgb(229, 227, 220);
    }

    .row {
        margin: 15px 0;
        white-space: nowrap;
    }

    .left {
        display: inline-block;
        width: 50%;
    }

    .right {
        display: inline-block;
        width: 50%;
    }

    input[type=range], input[type=color] {
        width: 100%;
    }

    .hidden {
        opacity: 0;
        pointer-events: none;
        transition: opacity 300ms ease-in-out;
    }

    .notransition {
        transition: none !important;
    }

    .lds-ring {
        display: inline-block;
        position: relative;
        width: 80px;
        height: 80px;
    }

    .lds-ring div {
        box-sizing: border-box;
        display: block;
        position: absolute;
        width: 64px;
        height: 64px;
        margin: 8px;
        border: 8px solid #444;
        border-radius: 50%;
        animation: lds-ring 1.2s cubic-bezier(0.5, 0, 0.5, 1) infinite;
        border-color: #444 transparent transparent transparent;
    }

    .lds-ring div:nth-child(1) {
        animation-delay: -0.45s;
    }

    .lds-ring div:nth-child(2) {
        animation-delay: -0.3s;
    }

    .lds-ring div:nth-child(3) {
        animation-delay: -0.15s;
    }

    @keyframes lds-ring {
        0% {
            transform: rotate(0deg);
        }
        100% {
            transform: rotate(360deg);
        }
    }

    #container {
        position: relative;
        height: 100vh;
    }

    #content {
        padding: 15px;
        font-size: 15px;
    }


    #title {
        font-family: 'Merriweather', serif;
        font-size: 24px;
        background: hsl(47, 15%, 66%);
        text-align: center;
        color: #222;
        -webkit-text-fill-color: transparent;
        -webkit-text-stroke: 1px #222;
        padding: 0px 0;
    }

    #loading {
        position: absolute;
        top: 0;
        right: 0;
        bottom: 0;
        left: 0;
        background: rgb(229, 227, 220);
        z-index: 1;
    }


    #loading .lds-ring {
        display: block;
        margin: 50px auto;
    }

    #footer {
        position: absolute;
        /*height: 30px;*/
        line-height: 30px;
        background: hsl(47, 15%, 66%);
        bottom: 0;
        width: 100vw;
        text-align: center;
        font-size: 13px;
    }

</style>
<body>

<div id="title">
    <h1>Violle</h1>
</div>
<div id="container">
    <div id="content">

        <div id="loading">
            <div class="lds-ring">
                <div></div>
                <div></div>
                <div></div>
                <div></div>
            </div>

        </div>

        <div class="row">
            <div class="left">
                <label for="brightness">Brightness</label>
            </div>
            <div class="right">
                <input type="range" class="control" id="brightness" min="0" max="255" value="0" step="1">
            </div>
        </div>

        <div class="row">
            <div class="left">
                <label for="outerBrightness">Outer brightness</label>
            </div>
            <div class="right">
                <input type="range" class="control" id="outerBrightness" min="0" max="255" value="0" step="1">
            </div>
        </div>

        <div class="row">
            <div class="left">
                <label for="hue">Hue</label></div>
            <div class="right">
                <input type="range" class="control" id="hue" min="0" max="255" value="255" step="1">
            </div>
        </div>
        <div class="row">
            <div class="left">
                <label for="saturation">Saturation</label></div>
            <div class="right">
                <input type="range" class="control" id="saturation" min="0" max="255" value="0" step="1">
            </div>


        </div>
        <div class="row">
            <div class="left">
                <label for="speed">Speed</label></div>
            <div class="right">
                <input type="range" class="control" id="speed" min="0" max="255" value="0" step="1">
            </div>
        </div>

    </div>
</div>

<div id="footer">
    Temperature: <span id="temperature">??</span>&deg;C
</div>

<script>
    const ids = ["brightness", "outerBrightness", "hue", "saturation", "speed"];
    const gateway = `ws://${window.location.hostname}/ws`;
    let websocket;
    window.addEventListener('load', onLoad);

    function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
    }

    function onOpen(event) {
        console.log('Connection opened');
        toggleLoading(false);
    }

    function onClose(event) {
        console.log('Connection closed');
        toggleLoading(true);
        setTimeout(initWebSocket, 1000);
    }

    function onMessage(event) {
        console.log('Received: ' + event.data);
        let data = JSON.parse(event.data);

        ids.forEach(id => {
            if (data.hasOwnProperty(id))
                document.getElementById(id).value = data[id].toString(10);
        });

        if (data.hasOwnProperty("temperature"))
            document.getElementById("temperature").innerText = Math.round(data.temperature).toString(10);

    }

    function onLoad(event) {
        // setTimeout(() => toggleLoading(false), 100);
        initWebSocket();

        ids.forEach(id => {
            document.getElementById(id).addEventListener("input", (evt) => send());
        })
    }

    let timeoutId;
    function send() {
        clearTimeout(timeoutId);
        timeoutId = setTimeout(() => {
            let obj = Object.fromEntries(ids.map(id => [id, parseInt(document.getElementById(id).value, 10)]));
            let json = JSON.stringify(obj);

            if (websocket)
                websocket.send(json);
            else
                console.log(obj);
        }, 10)
    }

    function toggleLoading(state) {
        document.getElementById("loading").classList.toggle("hidden", !state);
    }
</script>
</body>
</html>
)rawliteral";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
StaticJsonDocument<JSON_OBJECT_SIZE(5)> doc;
StaticJsonDocument<JSON_OBJECT_SIZE(1)> temperatureDoc;

CRGB leds[NUM_LEDS];

CRGB a = CRGB::Red;
CRGB b = CRGB::Blue;

CRGB palette[5];

uint8_t outerBrightness;
uint8_t hue;
uint8_t saturation;
uint8_t speed;
int ticks;
int temperatureTicks;

float temperature; 

void initOTA() { 
  ArduinoOTA.setHostname("violle");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();  

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void notifyClients() {
  String output = "";
  doc["brightness"] = FastLED.getBrightness();
  doc["outerBrightness"] = outerBrightness;
  doc["hue"] = hue;
  doc["saturation"] = saturation;
  doc["speed"] = speed;
  serializeJson(doc, output);
  ws.textAll(output);
}

void notifyClientsTemperature() {
  String output = "";
  temperatureDoc["temperature"] = ((analogRead(TEMPERATURE_PIN) / 1023.0) - 0.5)  * 100;
  serializeJson(temperatureDoc, output);
  ws.textAll(output);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    deserializeJson(doc, data);
    FastLED.setBrightness(doc["brightness"]);
    outerBrightness = doc["outerBrightness"];
    hue = doc["hue"];
    saturation = doc["saturation"];
    speed = doc["speed"];   
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {  
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients();
      notifyClientsTemperature();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


void initLEDs() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);

  ticks = 0;
  hue = random(0, 256);
  saturation = 0;
  outerBrightness = 128;
  speed = 50;
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  initOTA();

  initWebSocket();
  initWebServer();

  initLEDs();
}


void loopOTA() {
  // OTA
  ArduinoOTA.handle();
}

void loopLEDs() {
    // LED
  fill_gradient(palette, 5, CHSV(hue, saturation, 255), CHSV(hue, 255, outerBrightness), FORWARD_HUES); 
  
  for (int i = 0; i < 4; i++) {
    leds[i] = palette[0];
  }

  for (int i = 4; i < 32; i++) {
    leds[i] = palette[1];
  }

  for (int i = 32; i < 84; i++) {
    leds[i] = palette[2];
  }

  for (int i = 84; i < 164; i++) {
    leds[i] = palette[3];
  }
  
  for (int i = 164; i < NUM_LEDS; i++) {
    leds[i] = palette[4];
  }
 
  FastLED.show();


  if (speed > 0) {
    ticks++;
    if (ticks > 255 - speed) {
      ticks = 0;
       
      hue++;
      if(hue > 255) 
        hue = 0;
    }
  }


  temperatureTicks++;
  if (temperatureTicks > 500) {
    temperatureTicks = 0;
    
    notifyClientsTemperature();
  }
}


void loop() {
  loopOTA();
  loopLEDs();
}
