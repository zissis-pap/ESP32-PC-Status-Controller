<h1>ESP32_PC_Status_Controller</h1>
<b><ul><i>The readme file is not finished yet. The circuit will be uploaded soon.</i></ul></b>

This project aims at providing additional Power-On, Shutdown, Reset and System Halt capabilities to a Desktop Computer. You can now power on or shutdown your pc via a web interface based on node-red.

<h3>System Function</h3>
<p>The system is built with the <a href="https://www.espressif.com/en/products/hardware/esp32/overview">ESP32 microcontroller</a> and is connected to the computer's motherboard front-panel header. It is also connected to the Computer's case power-on and reset switches. In order to use the remote capabilities, a system with <a href="https://nodered.org/">node-red</a> and <a href="https://mqtt.org/">mqtt broker</a> is required (you may use a R-PI for that). The system itself is composed of an ESP32 microcontroller, a 32x8 Dot Matrix Led Display and 2ch, 5v Power-Relay module.</p> 

<h4>The node-red interface</h4>
<p>The node-red interface connects with the ESP32 through the MQTT broker. Pointing your browser at your node-red's host ui IP returns the following dashboard:</p>
<img src="https://user-images.githubusercontent.com/11696874/79850830-2164a600-83cd-11ea-9094-877fbcbbf44c.png">
<p>There is a total of 10 elements on the dashboard and are organised in three sections according to their function. The first one is the Display where the pc status and the actions of the microcontroller are displayed.</p>
<p>The display prints info about the pc status, for example, if the computer is off it prints the "PC is off" message. It also prints the actions taken or requested by the microcontroller.</p>
<img src="https://user-images.githubusercontent.com/11696874/79852466-5d006f80-83cf-11ea-9b57-7f895436ae4e.png">

<p>The next section is about the PC power functions</p>
<p>In this section, the user may power-on, power-off, reset or halt their computer</p>
<img src="https://user-images.githubusercontent.com/11696874/79852846-e7e16a00-83cf-11ea-8e07-f7e3cc70c120.png">

<p>The final section contains the extra features of the PC-Status-Controller</p>
<p>In this section the user may print the IP addresses of both the Controller and the MQTT broker. They may also print the date and time, see when the computer was last turned on and off and update the Controller's firmware using the OTA feature.</p> 
<img src="https://user-images.githubusercontent.com/11696874/79853047-37c03100-83d0-11ea-8592-7ca45eac81d7.png">


<h3>Required Libraries</h3>
<ul>
  <li><a href="https://github.com/arduino-libraries/NTPClient">NTP Client Library</a>
  <li><a href="https://github.com/knolleary/pubsubclient">MQTT Client Library</a>
  <li><a href="https://github.com/MajicDesigns/MD_MAX72XX">MD_MAX72XX Library</a>
  
</ul>
