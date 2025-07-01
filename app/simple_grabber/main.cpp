// include soliti
#include <iostream>
#include <fstream>
#include <signal.h>

// librerie lidar
#include <rplidar.h>
//#include <sl_lidar.h>
//#include <sl_lidar_driver.h>

// libreria ZMQ
#include <zmq.hpp> // Assicurati che zmq.hpp sia accessibile (di solito in /usr/local/include o simile)
#include <thread>  // Per std::this_thread::sleep_for
#include <chrono>  // Per std::chrono::milliseconds

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

// namespace di slamtec e std
using namespace sl;
using namespace std;

// create the driver instance
ILidarDriver * lidar;

// porta seriale
IChannel* channel;

void ctrlc(int signum);

void stop_lidar(string msg);
void check_health();
void init_lidar();

void stop_lidar(string msg=""){
    cerr<<msg<<"\n";

    lidar->stop();
    delay(20);

    lidar->setMotorSpeed(0);

    if(lidar) {
        delete lidar;
        lidar = NULL;
    }

    if(channel) {
        delete channel;
        channel = NULL;
    }

    exit(0);
}

// Funzione per gestire il segnale Ctrl+C
void ctrlc(int signum){
    cout<<"CTRL-C pressed. Closing...\n";
    stop_lidar();
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

void init_lidar(){
    
    channel = *createSerialPortChannel("/dev/tty.usbserial-11110", 460800);

    ///  Create a LIDAR driver instance
    lidar = *createLidarDriver();
    auto res = lidar->connect(channel);

    if(SL_IS_OK(res)){
        sl_lidar_response_device_info_t deviceInfo;
        res = lidar->getDeviceInfo(deviceInfo);
        if(SL_IS_OK(res)){
            printf("Model: %d, Firmware Version: %d.%d, Hardware Version: %d\n",
            deviceInfo.model,
            deviceInfo.firmware_version >> 8, deviceInfo.firmware_version & 0xffu,
            deviceInfo.hardware_version);
        }else{
            fprintf(stderr, "Failed to get device information from LIDAR %08x\r\n", res);
        }
    }else{
        fprintf(stderr, "Failed to connect to LIDAR %08x\r\n", res);
    }

    // accende il motore
    lidar->setMotorSpeed();

    LidarScanMode scanMode;

    if (SL_IS_FAIL( lidar->startScan(false, true, 0, &scanMode) )) // you can force slamtec lidar to perform scan operation regardless whether the motor is rotating
        stop_lidar("Error: cannot start the scan operation.");
}

struct LidarPoint {
    float angle;
    float distance;

    LidarPoint() {};
    LidarPoint(float a, float d): angle(a), distance(d) {};
};

struct TimeDiff {
    float start, end, diff;

    TimeDiff() {};
    TimeDiff(float s, float e, float d): start(s), end(e), diff(d) {};
};

int main(){

    init_lidar();

    signal(SIGINT, ctrlc);
    delay(3000);

    // 1. Inizializza il contesto ZeroMQ
    zmq::context_t context(1); // 1 thread IO

    // 2. Crea un socket PUBLISHER
    zmq::socket_t publisher(context, zmq::socket_type::pub);

    // Lega il publisher a un indirizzo. IPC è per la comunicazione locale veloce.
    // Puoi anche usare "tcp://*:5556" per la comunicazione di rete.
    publisher.bind("ipc:///tmp/lidar_data"); 
    // publisher.bind("tcp://*:5556"); // Alternativa per rete

    cout << "Publisher C++ avviato. Invio dati LiDAR..." << endl;
    vector<LidarPoint> data;

    int data_count = 0;

    while(1){

        sl_lidar_response_measurement_node_hq_t nodes[8192];
        size_t count = 8192;
        sl_result res = lidar->grabScanDataHq(nodes, count, 1000);

        if (SL_IS_OK(res)) {
            lidar->ascendScanData(nodes, count);

            data.clear();
            data.reserve(500);

            float angle, dist;
            /* float last_angle = 0, diff;
            vector<TimeDiff> diffs; */

            for (int pos = 0; pos < (int)count ; pos++){
                
                angle = ((nodes[pos].angle_z_q14 * 90.f) / 16384.f);
                dist = (nodes[pos].dist_mm_q2/4.0f);

                //if(dist < 1e-4) continue;

                data.emplace_back(angle, dist);
        
                //file<<angle<<" "<<dist<<"\n";
                /* diff = abs(angle-last_angle);
                if(diff > 10)
                    diffs.emplace_back(last_angle, angle, diff);
                
                last_angle = angle; */
            }

            /* angle = data[0].angle;
            diff = 360-abs(angle-last_angle);
            if(diff > 10)
                diffs.emplace_back(last_angle, angle, diff); */

            // Serializza i dati
            // Copia i dati dal vector direttamente in un messaggio ZMQ
            // Ogni LidarPoint ha 2 float, ogni float sono 4 byte. Quindi 2 * 4 = 8 byte per punto.
            size_t data_size = data.size() * sizeof(LidarPoint);
            zmq::message_t message(data_size);
            memcpy(message.data(), data.data(), data_size);

            // Invia i dati tramite il socket PUBLISHER
            publisher.send(message, zmq::send_flags::none);
            
            //for(auto &d:diffs)
            //    cout<<"Diff "<<d.start<<"-"<<d.end<<" ("<<d.diff<<")\n";
            cout<<"Inviati "<<data.size()<<" punti ("<<(++data_count)<<")\n";
            

        } else {
            cout<<"Error: "<<res<<"\n";
            stop_lidar();
        }
    }

    return 0;
}