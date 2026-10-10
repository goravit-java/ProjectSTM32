Smart Vending Monitor - โปรแกรม Serial Monitor แบบ Terminal UI (Python + Textual)
==================================================================================

ไฟล์ในโฟลเดอร์นี้
  vending_tui.py     โปรแกรมหลัก
  demo_trace.txt     ข้อมูล UART ที่ได้จากการจำลอง Firmware จริง (ใช้กับโหมด --demo)
  requirements.txt   ไลบรารีที่ต้องติดตั้ง

1) ติดตั้ง (Python 3.9 ขึ้นไป)
   pip install -r requirements.txt

2) อัปเดต Firmware ก่อน (สำคัญ)
   TUI อ่านบรรทัดสถานะ "#S ..." ที่ Firmware ส่งออกมา ต้อง Flash โปรเจกต์ชุดใหม่ที่มี
   Src/App/telemetry.c + Inc/App/telemetry.h และ main.c ที่เรียก Telemetry_Update()
   (ถ้าใช้ Firmware เก่า TUI ยังแสดง Log ได้ แต่ Dashboard จะขึ้น "waiting #S")

3) วิธีรัน
   python vending_tui.py                    หาพอร์ต ST-LINK ของบอร์ด NUCLEO ให้เอง
   python vending_tui.py --list             ดูรายชื่อพอร์ตทั้งหมด
   python vending_tui.py --port COM5        Windows
   python vending_tui.py --port /dev/cu.usbmodem1103    macOS
   python vending_tui.py --demo             เล่นข้อมูลตัวอย่าง ไม่ต้องต่อบอร์ด
   python vending_tui.py --demo --speed 3   เล่นเร็วขึ้น 3 เท่า

   หมายเหตุ: ปิด Serial Terminal ตัวอื่น (เช่น PuTTY, CubeIDE Console) ก่อน เพราะพอร์ตเปิดได้ทีละโปรแกรม
   แนะนำให้ขยายหน้าต่าง Terminal อย่างน้อย 120 x 36 ตัวอักษร (หรือเต็มจอ)

4) หน้าจอ
   แท็บ Dashboard
     Machine        State ของ FSM, สถานะ Lockout, เกณฑ์ที่ใช้อยู่ (Temp / Humid), พัดลม (PC2), Telemetry
     Environment    อุณหภูมิ NTC และความชื้น DHT11 พร้อมแถบเทียบเกณฑ์ที่ตั้งไว้ และกราฟย่อ
     Current order  (หน้า SETTINGS: ค่าที่กำลังตั้ง) สินค้า ราคา ยอดจ่าย ยอดขาด จำนวนครั้งที่ต้องบังแสง เวลาที่เหลือ ความคืบหน้าการจ่ายสินค้า
     OLED mirror    จำลองหน้าจอ OLED ตามตรรกะเดียวกับ display.c
     Stock          ตารางสินค้า (อัปเดตจากบอร์ดทันทีที่สต็อกเปลี่ยน)
     Session        ยอดขาย รายได้ เงินทอน จ่ายไม่สำเร็จ ยกเลิก ครั้งที่ล็อก เซ็นเซอร์ผิดพลาด
     Recent events  เหตุการณ์สำคัญ แยกสี
   แท็บ UART Log    Log ดิบทั้งหมด (เปิดสวิตช์เพื่อดูบรรทัด #S ได้)
   แท็บ Trends      กราฟอุณหภูมิ/ความชื้นย้อนหลัง 5 นาที พร้อม min / max

5) ปุ่มลัด (ใช้เมาส์คลิกปุ่มด้านบนหรือแท็บได้เช่นกัน)
   1 / 2 / 3  สลับแท็บ        r  เชื่อมต่อใหม่        c  ล้าง Log
   s  บันทึก Log เป็นไฟล์      p  หยุด/เลื่อน Log ต่อ   t  แสดง/ซ่อนบรรทัด #S
   d  สลับธีมสว่าง/มืด          q  ออกจากโปรแกรม

6) รูปแบบบรรทัดสถานะจาก Firmware (ส่งเมื่อค่าเปลี่ยน ห่างกันอย่างน้อย 200 ms และทุก 5 วินาที)
   #S state=PAYMENT temp=27.4 tlim=40.0 hlim=70 hum=55 sens=1 lock=0 item=0 price=25 paid=20 left=24 prog=0
      cancel=0 sf=0 spt=40.0 sph=70 stock=3,2,1,0

   state   INIT, IDLE, SELECT, CONFIRM, CHECK_STOCK, PAYMENT, PAYMENT_FAILED,
           SAFETY_CHECK, PROCESSING, COMPLETE, SETTINGS, FAULT
   temp    อุณหภูมิ °C (NTC ที่ PA0)      tlim   เกณฑ์อุณหภูมิที่ใช้อยู่ (20.0-50.0 °C)
   hlim    เกณฑ์ความชื้นที่ใช้อยู่ (40-90 %)
   hum     ความชื้น % (DHT11)
   sens    1 = มีค่าเซ็นเซอร์แล้ว         lock   1 = Safety Lockout
   item    index สินค้าที่เลือก (0-3)      price  ราคาสินค้านั้น (บาท)
   paid    ยอดที่จ่ายแล้ว                 left   วินาทีที่เหลือ (PAYMENT/PROCESSING)
   prog    ความคืบหน้าการจ่ายสินค้า %     cancel 1 = รายการถูกยกเลิกเพราะล็อก
   sf      หน้า SETTINGS: หัวข้อที่เลือก (0 = อุณหภูมิ, 1 = ความชื้น)
   spt/sph หน้า SETTINGS: ค่าอุณหภูมิ / ความชื้นที่กำลังตั้ง (ยังไม่บันทึก)
   stock   สต็อกของสินค้าทั้ง 4 รายการ
