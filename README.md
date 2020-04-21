<h1>ESP32_PC_Status_Controller</h1>
<b><ul><i>The readme file is not finished yet. The circuit will be uploaded soon.</i></ul></b>

This project aims at providing additional Power-On, Shutdown, Reset and System Halt capabilities to a Desktop Computer. You can now power on or shutdown your pc via a web interface based on node-red.

<h3>System Function</h3>
<p>The system is built with the <a href="https://www.espressif.com/en/products/hardware/esp32/overview">ESP32 microcontroller</a> and is connected to the computer's motherboard front-panel header. It is also connected to the Computer's case power-on and reset switches. In order to use the remote capabilities, a system with <a href="https://nodered.org/">node-red</a> and <a href="https://mqtt.org/">mqtt broker</a> is required (you may use a R-PI for that). The system itself is composed of an ESP32 microcontroller, a 32x8 Dot Matrix Led Display and 2ch, 5v Power-Relay module.</p> 

<h4>The node-red interface</h4>
<p>The node-red interface connects with the ESP32 through the MQTT broker. Pointing your browser at your node-red's host ui IP returns the following dashboard:</p>


<h3>Required Libraries</h3>
<ul>
  <li><a href="https://github.com/arduino-libraries/NTPClient">NTP Client Library</a>
  <li><a href="https://github.com/knolleary/pubsubclient">MQTT Client Library</a>
  <li><a href="https://github.com/MajicDesigns/MD_MAX72XX">MD_MAX72XX Library</a>
  
</ul>
