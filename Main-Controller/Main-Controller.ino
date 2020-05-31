// Arduino Mega2560 (Main Controller part)
// 1. Touch Screen                              [Done]
// 2. DHT Sensing - DC FAN, Display Info        [Done]
// 3. Door Control (Servo motor)                [Done]
// 4. Communication with Smartphone (Bluetooth) [Yet]
// 5. Motion Sensing                            [Yet]
// 6. Communication with Sub-Controller (UART)  [Done]
#include <SoftwareSerial.h>
#include <Servo.h>
#include <Nextion.h>
#include <DHT.h>

// pin numbering
const int motionPin = 3;
const int dhtPin = 4;
const int doorPin = 9;
const int btRx = 10;
const int btTx = 11;
const int dcfanPin = 44;
const int audioGuidancePin = 50;

// declaration class instances
SoftwareSerial btSerial(btTx, btRx);
Servo doorServo;
int doorAngle = 180;
DHT dhtModule(dhtPin, DHT11);

float humidity;

// enable/disable motion detect
boolean enableMotionDetect;
int not_motion_count;

boolean isDoorOpen;

// for touch screen (Nextion)
unsigned long t1_prev = 0;            // millis 함수을 위한 변수
unsigned long t2_prev = 0;
unsigned long t1_delay = 1000;
unsigned long t2_delay = 1000;

NexButton b0 = NexButton(0,5,"b0");   // 터치스크린의 button,text,picture 객체선언 
NexButton b1 = NexButton(0,6,"b1");
NexButton b2 = NexButton(0,9,"b2");
NexButton b3 = NexButton(0,10,"b3");

NexText t1 = NexText(0,1,"t1");
NexText t2 = NexText(0,2,"t2");
NexText t3 = NexText(0,8,"t3");

NexPicture p0 = NexPicture(0,3,"p0");
NexPicture p1 = NexPicture(0,4,"p1");

NexTouch *nex_listen_list[] = { // 터치했을때 이벤트가 발생하는 요소
    &b0,
    &b1,
    &b2,
    &b3,
    NULL
};

void doorOpenProc(){
    if(!isDoorOpen){
        digitalWrite(audioGuidancePin, HIGH);
        delay(10);
        digitalWrite(audioGuidancePin, LOW);

        doorAngle = 60;
        doorServo.write(doorAngle);
    }
    Serial.println("Door open!");
    isDoorOpen = true;
}

void doorCloseProc(){
    if(isDoorOpen){
        doorAngle = 180;
        doorServo.write(doorAngle);
    }
    Serial.println("Door Close!");
    isDoorOpen = false;
}

void step_left(){
    Serial.println("Stepper motor: CCW");
    Serial1.write(2);
}

void step_right(){
    Serial.println("Stepper motor: CW");
    Serial1.write(5);
}

void b0PopCallback(void *ptr) {  // b0 버튼(door open)
    doorOpenProc();
}

void b1PopCallback(void *ptr) {  // b1 버튼(door close)
    doorCloseProc();
}

void b2PopCallback(void *ptr) {  // b2 버튼(왼쪽 방향 버튼)
    step_left();
}  

void b3PopCallback(void *ptr) { // b3 버튼(오른쪽 방향 버튼)
    step_right();
}

void getHumidity() {            // 습도 측정
    humidity = dhtModule.readHumidity();
    char hTemp[10] = {0}; 
    utoa(int(humidity), hTemp, 10);
    t1.setText(hTemp); 
    Serial.println(humidity);
}

void on_off() {                 // 습도에 따른 제습모드 on_off  
    float h = dhtModule.readHumidity();
    if(h >= 70) {
        t3.setText("ON");       // fan도 작동
        digitalWrite(dcfanPin, HIGH);
    }
    else {
        t3.setText("OFF");
        digitalWrite(dcfanPin, LOW);
    }
}

void setup() {
    pinMode(dcfanPin, OUTPUT);
    dhtModule.begin();
    // for monitoring
    Serial.begin(9600);

    // Mega <-> Touch Screen
    Serial3.begin(9600);
    
    // Mega <-> Uno UART communication -> Serial1 object (19:Rx, 18:Tx)
    Serial1.begin(9600);

    // Mega <-> Smartphone Bluetooth communication
    btSerial.begin(9600);

    doorServo.attach(doorPin);
    doorServo.write(doorAngle);

    // touch screen
    nexInit();
    b0.attachPop(b0PopCallback,&b0);          
    b1.attachPop(b1PopCallback,&b1);
    b2.attachPop(b2PopCallback,&b2);
    b3.attachPop(b3PopCallback,&b3);

    enableMotionDetect = isDoorOpen = false;
    not_motion_count = 0;
}

const int command_bt_num = 6;
const String command_bt[command_bt_num] = {
    "stp_l", // Stepper left
    "stp_r", // Stepper right
    "srv_o", // Servo(Door) Open
    "srv_c", // Servo(Door) Close
    "m_ena", // Motion enable
    "m_dis"  // Motion disable
};

void loop() {
    // setting bluetooth module
    if(btSerial.available()){Serial.write(btSerial.read());}
    if(Serial.available()){btSerial.write(Serial.read());}

    unsigned long t1_now = millis();

    if(t1_now - t1_prev >= t1_delay) {
        t1_prev = t1_now;
        getHumidity();
        btSerial.print("H"+String.valueOf(humidity));
    }

    unsigned long t2_now = millis();

    if(t2_now - t2_prev >= t2_delay) {
        t2_prev = t2_now;
        on_off();
    }                                                                                                                                                                           

    nexLoop(nex_listen_list);

    // Sub -> Main 데이터 받아오기
    // 이 데이터를 다시 스마트폰으로 던져줄 것
    static String prevData, currentData;
    if(Serial1.available()){
        currentData = Serial1.readString();
        
        if(prevData != currentData){
            Serial.print(currentData);
            prevData = currentData;
        }

        btSerial.print(String("H")+String(humidity));
        btSerial.print(String("W")+currentData);
    }

    if(btSerial.available()){
        String read_data_bt = btSerial.readString();
        int now_command = -1;
        for(int i=0;i<command_bt_num;++i){
            if(!read_data_bt.compareTo(command_bt[i])){ // match
                now_command = i;
                break;
            }
        }

        switch(now_command){
        case 0: // Stepper left
            step_left();
            break;
        case 1: // Stepper right
            step_right();
            break;
        case 2: // Servo(Door) open
            doorOpenProc();
            break;
        case 3: // Servo(Door) close
            doorCloseProc();
            break;
        case 4: // Motion enable
            enableMotionDetect = true;
            break;
        case 5: // Motion disable
            enableMotionDetect = false;
            break;
        default:
        }
    }

    if(enableMotionDetect){
        // millis로 바꾸고 코드 수정해야함
        /*
        int motion_read = digitalRead(motionPin);
        Serial.print("motion info: ");
        Serial.print(motion_read);

        if(motion_read == HIGH){ // 모션 감지 될 때
            if(!isDoorOpen){ // 현재 옷장이 닫혀 있는 상태라면
                doorServo.write(60);
                isDoorOpen = true; // 옷장이 열려 있는 상태
            }
            not_motion_count = 0; // 모션이 감지되어 카운트 초기화
        }
        else {
            ++ not_motion_count; // 모션 감지 안 될 때 우선 카운트
        }

        delay(500);
        if(isDoorOpen && not_motion_count == 20){ // 열 번 카운트 하는 동안 모션 감지 안 되었으면
            for (servo_pos = 0; servo_pos <= 120; servo_pos += 1) { // 옷장 문 닫기
                myservo.write(servo_pos);
                delay(15);
            }
            not_motion_count = 0; // 카운트 초기화
            isDoorOpen = 0; // 옷장이 닫혀 있는 상태
        }
        Serial.print("\t");
        if(!isDoorOpen){ Serial.print("Closed");}
        else { Serial.print("Open!");}
        Serial.print("\tCount: ");
        Serial.println(not_motion_count);

        delay(500);*/
    }
}
