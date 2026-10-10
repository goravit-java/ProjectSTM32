# 🥦 Vegetable Vending Machine with Preservation System

An automated vegetable vending machine and environmental monitoring system built on the **STM32F401RE** microcontroller. This project combines state-machine architecture, multi-sensor feedback, motor actuators, and real-time OLED monitoring to deliver a reliable automated vending process alongside active freshness preservation.

---

## 📌 Project Overview
Vending fresh produce requires precise operational control and continuous environmental management. This system handles user selection, verifies payment input via optical sensors, dispenses items using motor actuation, and continuously monitors ambient conditions to trigger temperature-controlled cooling.

### Key Highlights
- **Finite State Machine (FSM) Control:** Ensures predictable state transitions across selection, payment, dispensing, and idle modes.
- **LDR-Based Payment Verification:** Optical sensing mechanism to detect coin/payment insertion.
- **Active Freshness Preservation:** Closed-loop monitoring that drives a 12V cooling fan via PWM to regulate internal temperature.
- **OLED User Interface:** Real-time feedback displaying item availability, prices, operating states, and sensor telemetry.

---

## 🛠️ Hardware Architecture & Peripherals

### Hardware Components
* **Microcontroller:** STM32F401RE (ARM Cortex-M4 @ 84 MHz)
* **Actuators:** Servo Motor (dispensing mechanism), DC Motor, 12V Cooling Fan, Relay Modules
* **Sensors:** LDR (Light Dependent Resistor), Analog Temperature Sensor
* **Display:** 0.96" OLED Display (I2C)
* **Input/Control:** Push Buttons for product selection and manual resets

### STM32 Peripherals Utilized
| Peripheral | Function / Usage |
| :--- | :--- |
| **GPIO** | Button inputs, status LEDs, and relay switching |
| **ADC** | Sampling analog inputs from temperature and light sensors |
| **TIM (PWM)** | Precise pulse-width modulation for servo positioning and fan speed control |
| **I2C** | Driving the OLED display interface |
| **UART / SPI** | Serial communication and peripheral expansion |
| **EXTI (Interrupts)** | Low-latency response for push-button inputs and coin detection events |

---

## 📐 System Logic & Architecture

```text
                       ┌─────────────────────────┐
                       │   Push Buttons (GPIO)   │
                       └────────────┬────────────┘
                                    │
┌────────────────────────┐          ▼          ┌────────────────────────┐
│ LDR Sensor (ADC/EXTI)  ├────► [ STM32 ] ────►│ OLED Display (I2C)     │
├────────────────────────┤      │ F401RE │     ├────────────────────────┤
│ Temp Sensor (ADC)      ├────► [ Core  ] ────►│ Servo / Motors (PWM)   │
└────────────────────────┘          │          ├────────────────────────┤
                                    │          │ 12V Fan & Relays (GPIO)│
                                    └──────────┴────────────────────────┘
```

### Operational States (FSM)
1. **IDLE / MONITORING:** Displays menu options on OLED while running background temperature checks.
2. **SELECTION:** Captures button input to select the desired vegetable item and display the required price.
3. **PAYMENT_VERIFICATION:** Waits for payment insertion detected via the LDR sensor threshold.
4. **DISPENSING:** Drives the servo/motor to release the selected product.
5. **PRESERVATION_ACTIVE:** Automatically engages the 12V fan via PWM when the internal temperature exceeds set limits.

---

## 📂 Repository Structure

```text
ProjectSTM32-VegetableMachine/
├── Core/
│   ├── Inc/                  # C/C++ Header files (.h)
│   └── Src/                  # Core application source code (.c)
├── Drivers/                  # STM32F4xx HAL and CMSIS hardware drivers
├── docs/                     # Circuit schematics, FSM diagrams, and documentation
│   └── system_architecture.png
├── .gitignore                # Git ignore configuration for STM32CubeIDE build artifacts
└── README.md                 # Project documentation
```

---

## 💻 Building & Flashing

### Prerequisites
* **IDE:** [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) (v1.10.0 or higher)
* **Hardware:** STM32F401RE Nucleo-64 Board + ST-Link Programmer

### Steps
1. **Clone the Repository:**
   ```bash
   git clone [https://github.com/goravit-p/ProjectSTM32-VegetableMachine.git](https://github.com/goravit-p/ProjectSTM32-VegetableMachine.git)
   ```
2. **Import Project:**
   * Open STM32CubeIDE.
   * Go to `File` ➔ `Import...` ➔ `General` ➔ `Existing Projects into Workspace`.
   * Browse to the cloned `ProjectSTM32-VegetableMachine` folder and click **Finish**.
3. **Build Firmware:**
   * Press `Ctrl + B` (or click `Project` ➔ `Build Project`) to compile the source files.
4. **Flash Hardware:**
   * Connect the STM32 Nucleo board via USB.
   * Click **Run** (`Ctrl + F11`) to flash the `.elf` binary to the MCU.

---

## 📸 Demo & Hardware Gallery

| Circuit Setup | OLED Interface | Hardware Assembly |
| :---: | :---: | :---: |
| *(Add Photo)* | *(Add Photo)* | *(Add Photo)* |

---

## 👨‍💻 Author

**Goravit Promvaree**
* **Education:** B.Eng. in Computer Engineering, Khon Kaen University
* **Email:** [goravit.p@kkumail.com](mailto:goravit.p@kkumail.com)
* **GitHub:** [@goravit-p](https://github.com/goravit-p)
