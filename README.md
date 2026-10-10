# 🥦 Vegetable Vending Machine with Preservation System

An automated vegetable vending machine and environmental monitoring system powered by **STM32F401RE** microcontroller. This project integrates multiple hardware peripherals, sensor modules, actuator controls, and state-machine logic designed in C/C++.

---

## 📌 Features & Key Capabilities
- **Automated Vending Logic:** Push-button interaction logic for selecting products and confirming transactions.
- **LDR-Based Payment Detection:** Optical sensing mechanism for verifying coin/payment input.
- **Environmental Control System:** Automated 12V preservation fan controlled via temperature thresholds to extend vegetable freshness.
- **OLED Status Display:** Real-time user interface showing operating states, product prices, and system status.
- **Hardware Actuation:** Motor and servo controls for vending dispensing mechanisms and relays for load management.

---

## 🛠️ Hardware Components & Peripherals Used

### **Microcontroller & Hardware**
- **MCU:** STM32F401RE (ARM Cortex-M4)
- **Actuators:** Servo Motor, DC Motor, 12V Cooling Fan, Relays
- **Sensors:** LDR (Light Dependent Resistor), Temperature Sensor
- **Display:** OLED Display (I2C)
- **Inputs:** Push Buttons for selection and control

### **STM32 Peripherals Utilized**
- **GPIO:** Digital I/O for buttons, LEDs, and relay actuation
- **ADC:** Analog-to-digital conversion for sensor data reading
- **Timers & PWM:** Precise PWM signal generation for motor/servo position control and fan speed regulation
- **I2C:** High-speed communication protocol for OLED display driving
- **SPI & UART:** Serial communication and peripheral interfacing
- **External Interrupts (EXTI):** Event-driven responses for button inputs and coin detection

---

## 📐 System Architecture & State Machine
