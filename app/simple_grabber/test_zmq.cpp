// lidar_publisher.cpp
#include <iostream>
#include <vector>
#include <zmq.hpp> // Assicurati che zmq.hpp sia accessibile (di solito in /usr/local/include o simile)
#include <thread>  // Per std::this_thread::sleep_for
#include <chrono>  // Per std::chrono::milliseconds
 
// Struttura per rappresentare un punto LiDAR
struct LidarPoint {
    float theta;
    float distance;
};

int main() {
    // 1. Inizializza il contesto ZeroMQ
    zmq::context_t context(1); // 1 thread IO

    // 2. Crea un socket PUBLISHER
    zmq::socket_t publisher(context, zmq::socket_type::pub);
    // Lega il publisher a un indirizzo. IPC è per la comunicazione locale veloce.
    // Puoi anche usare "tcp://*:5556" per la comunicazione di rete.
    publisher.bind("ipc:///tmp/lidar_data"); 
    // publisher.bind("tcp://*:5556"); // Alternativa per rete

    std::cout << "Publisher C++ avviato. Invio dati LiDAR..." << std::endl;

    int scan_count = 0;
    while (true) {
        // 3. Simula una scansione LiDAR (circa 400 punti)
        std::vector<LidarPoint> scan_data;
        scan_data.reserve(400); // Prealloca spazio per efficienza

        for (int i = 0; i < 400; ++i) {
            LidarPoint point;
            point.theta = (float)i * 0.9f; // Angolo da 0 a 359.1 gradi
            point.distance = (float)(100 + (i % 50) * 5 + (scan_count % 10) * 10); // Distanza variabile
            scan_data.push_back(point);
        }

        // 4. Serializza i dati
        // Copia i dati dal vector direttamente in un messaggio ZMQ
        // Ogni LidarPoint ha 2 float, ogni float sono 4 byte. Quindi 2 * 4 = 8 byte per punto.
        size_t data_size = scan_data.size() * sizeof(LidarPoint);
        zmq::message_t message(data_size);
        memcpy(message.data(), scan_data.data(), data_size);

        // 5. Invia i dati tramite il socket PUBLISHER
        publisher.send(message, zmq::send_flags::none);

        std::cout << "Inviata Scansione #" << ++scan_count << " (" << scan_data.size() << " punti)" << std::endl;

        // Piccola pausa per simulare il tempo tra scansioni e non sovraccaricare
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ogni 100ms
    }

    // La chiusura del socket e del contesto avviene automaticamente quando escono dallo scope
    return 0;
}