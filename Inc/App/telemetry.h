#ifndef TELEMETRY_H_
#define TELEMETRY_H_

/* ส่งบรรทัดสถานะของระบบออก UART2 ให้โปรแกรม Serial Monitor (Python TUI) อ่านไปแสดงผลแบบ Real-time
 * รูปแบบ (ขึ้นต้นด้วย "#S " เพื่อแยกจากข้อความ Log ปกติ):
 *   #S state=PAYMENT temp=27.4 tlim=40.0 hlim=70 hum=55 sens=1 lock=0 item=0 price=25 paid=20 left=24 prog=0
 *      cancel=0 sf=0 spt=40.0 sph=70 stock=3,2,1,0
 *   (sf/spt/sph = หัวข้อและค่าที่กำลังตั้งในหน้า SETTINGS)
 * ส่งเมื่อค่าใดค่าหนึ่งเปลี่ยน (เว้นห่างอย่างน้อย 200 ms) และส่งซ้ำทุก 5 วินาทีเป็น Heartbeat
 */
void Telemetry_Update(void);   /* เรียกทุกรอบของ Main Loop (ไม่ Block: แค่ใส่ข้อความลง Ring Buffer ของ UART) */

#endif /* TELEMETRY_H_ */
