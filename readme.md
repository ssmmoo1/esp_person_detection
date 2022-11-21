Setup Steps

Clone Repository 

Use VS Code and open the esp_person_detection/examples/person_detection folder 

Open the SDK config and set  

- Camera Configuration to ESP32-CAM by AI-Thinker

- WiFi SSID and WiFi Password 

- SPI RAM Config - SPI RAM Access method set to Make RAM allocatable using malloc() 

Save flash and run 

Should see a person score output based on the camera in the terminal about twice per second and an attempt to connect to wifi. 
