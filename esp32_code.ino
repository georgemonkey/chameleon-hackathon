#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

WebServer server(80);

String getHTML(float tF, float hum){
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0"/>
<title>Mosquito Guard</title>
<style>
  :root{
    --bg-dark: #000a0e;
    --bg: #0a1a20;
    --bg-light: #0d2a30;
    --text: #8ffcff;
    --text-muted: #67c2c8;
    --highlight: #1ea8bb;
    --border: #0f7d8f;
    --border-muted: #0b4d5a;
    --primary: #06dff8;
    --secondary: #f7796a;
    --danger: #f7796a;
    --warning: #cda619;
    --success: #24cc7a;
    --info: #619bff;

    --temp: )rawliteral" + String(tF, 1) + R"rawliteral(;
    --humid: )rawliteral" + String(hum, 1) + R"rawliteral(;
    --battery: 67;
    --location: "Sammamish, WA";
  }

  body{
    margin:0;
    background:var(--bg-dark);
    font:16px/1.4 ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,Arial;
    color:var(--text);
  }
  .wrap{max-width:1100px;margin:auto;padding:24px;}
  .risk{
    background:var(--bg-light);
    border:1px solid var(--border);
    border-radius:16px;
    padding:28px 20px;
    text-align:center;
    margin-bottom:28px;
  }
  .risk .title{font-size:18px;color:var(--text-muted);margin-bottom:6px}
  .risk .value{font-size:56px;font-weight:800}
  .bar{
    height:22px;background:var(--border-muted);
    border-radius:999px;overflow:hidden;
    margin-top:18px;
  }
  .fill{
    height:100%;
    width:calc(var(--risk) * 1%);
    background:linear-gradient(90deg,var(--success),var(--warning),var(--danger));
  }
  .grid{
    display:grid;
    grid-template-columns:repeat(2,1fr);
    gap:28px;margin:0 0 32px;
  }
  .card{
    background:var(--bg);
    border:1px solid var(--border);
    border-radius:18px;
    padding:42px 28px;
    text-align:center;
    font-size:42px;
    font-weight:800;
  }
  .card small{
    display:block;
    margin-top:10px;
    font-size:18px;
    font-weight:600;
    color:var(--text-muted);
  }
  .chart-wrap{
    background:var(--bg);
    border:1px solid var(--border);
    border-radius:18px;
    padding:16px;
    height:360px;
  }
  .chart-wrap canvas{
    width:100%!important;
    height:100%!important;
  }
  @media (max-width:720px){
    .grid{grid-template-columns:1fr}
    .card{font-size:36px}
    .chart-wrap{height:320px}
  }
</style>
</head>
<body>
<div class="wrap">
  <h1>Mosquito Guard</h1>
  <section class="risk">
    <div class="title">Mosquito Breeding Risk</div>
    <div class="value" id="riskTxt"></div>
    <div class="bar"><div class="fill"></div></div>
  </section>
  <section class="grid">
    <div class="card" id="tempCard"></div>
    <div class="card" id="humidCard"></div>
    <div class="card" id="battCard"></div>
    <div class="card" id="locCard"></div>
  </section>
  <section class="chart-wrap">
    <canvas id="envChart"></canvas>
  </section>
</div>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script>
  function css(name){
    return getComputedStyle(document.documentElement).getPropertyValue(name).replace(/"/g,"").trim();
  }
  function updateRisk() {
    const temp = css("--temp"), humid = css("--humid"),
          batt = css("--battery"), loc = css("--location");
    const tempF = parseFloat(temp);
    const humidityPercent = parseFloat(humid);

    let temp_factor;
    if (tempF < 50) temp_factor = 0;
    else if (tempF < 75) temp_factor = (tempF - 50)/25*50;
    else if (tempF <= 95) temp_factor = 50 + (tempF - 75)/20*40;
    else if (tempF <= 100) temp_factor = 90 - (tempF - 95)/5*40;
    else temp_factor = 10;

    let humidity_factor;
    if (humidityPercent < 30) humidity_factor = 0;
    else if (humidityPercent < 42) humidity_factor = (humidityPercent - 30)/12*50;
    else if (humidityPercent <= 80) humidity_factor = 50 + (humidityPercent - 42)/38*40;
    else humidity_factor = 90;

    let chance = 0.6*temp_factor + 0.4*humidity_factor;
    chance = Math.min(100, Math.max(0, chance));
    chance = Math.round(chance*100)/100;

    document.documentElement.style.setProperty("--risk", chance);

    document.getElementById("riskTxt").textContent = `${chance}%`;
    document.getElementById("tempCard").innerHTML  = `${temp}°F<small>Temperature</small>`;
    document.getElementById("humidCard").innerHTML = `${humid}%<small>Humidity</small>`;
    document.getElementById("battCard").innerHTML  = `${batt}%<small>Battery</small>`;
    document.getElementById("locCard").innerHTML   = `${loc}<small>Location</small>`;
  }

  updateRisk();
  setInterval(updateRisk, 1000);

  const LABELS = ["-12m","-10m","-8m","-6m","-4m","-2m","Now"];
  const TEMP_SERIES  = [29.6,29.9,30.1,30.0,30.1,28, parseFloat(css("--temp"))];
  const HUMID_SERIES = [84,85,86,87,88,89, parseFloat(css("--humid"))];

  new Chart(document.getElementById("envChart").getContext("2d"), {
    type: "line",
    data: {
      labels: LABELS,
      datasets: [
        {
          label: "Temperature (°F)",
          data: TEMP_SERIES,
          yAxisID: "yTemp",
          borderColor: getComputedStyle(document.documentElement).getPropertyValue('--danger').trim(),
          backgroundColor: "transparent",
          borderWidth: 3,
          tension: 0.35
        },
        {
          label: "Humidity (%)",
          data: HUMID_SERIES,
          yAxisID: "yHum",
          borderColor: getComputedStyle(document.documentElement).getPropertyValue('--primary').trim(),
          backgroundColor: "transparent",
          borderWidth: 3,
          tension: 0.35
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: "index", intersect: false },
      scales: {
        yTemp: {
          type: "linear",
          position: "left",
          min: 24, max: 36,
          grid: { color: getComputedStyle(document.documentElement).getPropertyValue('--border-muted').trim() },
          ticks: { color: getComputedStyle(document.documentElement).getPropertyValue('--text-muted').trim() }
        },
        yHum: {
          type: "linear",
          position: "right",
          min: 70, max: 100,
          grid: { drawOnChartArea: false },
          ticks: { color: getComputedStyle(document.documentElement).getPropertyValue('--text-muted').trim() }
        },
        x: {
          grid: { color: getComputedStyle(document.documentElement).getPropertyValue('--border-muted').trim() },
          ticks: { color: getComputedStyle(document.documentElement).getPropertyValue('--text-muted').trim() }
        }
      },
      plugins: {
        legend: { labels: { color: getComputedStyle(document.documentElement).getPropertyValue('--text').trim() } }
      }
    }
  });
</script>
</body>
</html>
)rawliteral";
  return page;
}


void handleRoot(){
  float hum = dht.readHumidity();
  float tF = dht.readTemperature(true);
  if(isnan(hum) || isnan(tF)){ hum=0; tF=0; }
  server.send(200, "text/html", getHTML(tF, hum));
}

void setup(){
  Serial.begin(115200);
  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("SSD1306 failed");
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // start AP
  WiFi.softAP("MosquitoGuard67","12345678"); // password can be empty ""
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(ip);
  

  // show IP on OLED
  display.setCursor(0,0);
  display.println("WiFi AP started!");
  display.println("SSID: MosquitoGuard67");
  display.print("IP: "); display.println(ip);
  display.display();

  server.on("/", handleRoot);
  server.begin();
}

void loop(){
  server.handleClient();
}
