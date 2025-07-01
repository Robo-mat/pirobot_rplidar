// include soliti
#include <iostream>
#include <fstream>

// librerie lidar
#include "sl_lidar.h" 
#include "sl_lidar_driver.h"

// trova la dimensione di un array
#define get_size(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))

// roba Mac/Linux
#include <unistd.h>

// sleep per ms
static inline void delay(sl_word_size_t ms){
    while (ms>=1000){
        usleep(1000*1000);
        ms-=1000;
    };
    if (ms!=0)
        usleep(ms*1000);
}

// namespace di slamtec
using namespace sl;
using namespace std;

// create the driver instance
ILidarDriver * lidar;

// porta seriale
IChannel* channel;

void stop_lidar(string msg=""){
    cerr<<msg<<"\n";

    lidar->stop();
    delay(20);

    lidar->setMotorSpeed(0);

    if(lidar) {
        delete lidar;
        lidar = NULL;
    }
    exit(0);
}

void check_health(){
    // calcola health info e controlla che tutto sia a posto
    sl_lidar_response_device_health_t healthinfo;

    if (SL_IS_FAIL( lidar->getHealth(healthinfo) ))
        stop_lidar("Error: cannot retrieve the lidar health code");

    if (healthinfo.status == SL_LIDAR_STATUS_ERROR)
        stop_lidar("Error: slamtec lidar internal error detected. Please reboot the device to retry.");
}

int main(){

    lidar = *createLidarDriver();

    if (!lidar) {
        cout<<"Error: insufficent memory\n";
        return -2;
    }

    channel = *createSerialPortChannel("/dev/tty.usbserial-1110", 460800);

    if (SL_IS_FAIL( lidar->connect(channel) )) {
        // errore di connessione
        cerr<<"Errore di connessione\n";
        stop_lidar();
    }

    check_health();

    // accende il motore
    lidar->setMotorSpeed();

    if (SL_IS_FAIL( lidar->startScan(0, 1) )) // you can force slamtec lidar to perform scan operation regardless whether the motor is rotating
        stop_lidar("Error: cannot start the scan operation.");

    delay(3000);

    sl_lidar_response_measurement_node_hq_t nodes[8192];
    size_t count = get_size(nodes);

    sl_result res = lidar->grabScanDataHq(nodes, count, 0);
    delay(1000);

    if (SL_IS_OK(res) || res == SL_RESULT_OPERATION_TIMEOUT) {
        lidar->ascendScanData(nodes, count);

        ofstream file("data.txt");
        for (int pos = 0; pos < (int)count ; pos++)
            file<<((nodes[pos].angle_z_q14 * 90.f) / 16384.f)<<" "<<(nodes[pos].dist_mm_q2/4.0f)<<"\n";
        
        file.flush();
        file.close();
        stop_lidar();
    
    } else {
        cout<<"Error: "<<res<<"\n";
        stop_lidar();
    }

    return 0;
}