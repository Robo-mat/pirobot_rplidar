// include soliti
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// librerie lidar
#include "sl_lidar.h" 
#include "sl_lidar_driver.h"

// trova la dimensione di un array
#ifndef _countof
#define _countof(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))
#endif

// roba Windows
#ifdef _WIN32
#include <Windows.h>
#define delay(x)   ::Sleep(x)
#else

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
#endif

// namespace di slamtec
using namespace sl;

// normale printf
void print_usage(int argc, const char * argv[])
{
    printf("Simple LIDAR data grabber for SLAMTEC LIDAR.\n"
           "Version:  %s \n"
           "Usage:\n"
           " For serial channel %s --channel --serial <com port> [baudrate]\n"
           " The baudrate used by different models is as follows:\n"
           "  A1(115200),A2M7(256000),A2M8(115200),A2M12(256000),"
           "A3(256000),S1(256000),S2(1000000),S3(1000000)\n"
		   " For udp channel %s --channel --udp <ipaddr> [port NO.]\n"
           "The LPX default ipaddr is 192.168.11.2,and the port NO.is 8089. Please refer to the datasheet for details.\n"
           , SL_LIDAR_SDK_VERSION,  argv[0], argv[0]);
}

 
void plot_histogram(sl_lidar_response_measurement_node_hq_t * nodes, size_t count)
{
    const int BARCOUNT =  100;
    const int MAXBARHEIGHT = 20;
    // const float ANGLESCALE = 360.0f/BARCOUNT;

    float histogram[BARCOUNT];
    for (int pos = 0; pos < _countof(histogram); ++pos) {
        histogram[pos] = 0.0f;
    }

    float max_val = 0;
    for (int pos =0 ; pos < (int)count; ++pos) {
        int int_deg = (int)(nodes[pos].angle_z_q14 * 90.f / 16384.f);
        if (int_deg >= BARCOUNT) int_deg = 0;
        float cachedd = histogram[int_deg];
        if (cachedd == 0.0f ) {
            cachedd = nodes[pos].dist_mm_q2/4.0f;
        } else {
            cachedd = (nodes[pos].dist_mm_q2/4.0f + cachedd)/2.0f;
        }

        if (cachedd > max_val) max_val = cachedd;
        histogram[int_deg] = cachedd;
    }

    for (int height = 0; height < MAXBARHEIGHT; ++height) {
        float threshold_h = (MAXBARHEIGHT - height - 1) * (max_val/MAXBARHEIGHT);
        for (int xpos = 0; xpos < BARCOUNT; ++xpos) {
            if (histogram[xpos] >= threshold_h) {
                putc('*', stdout);
            }else {
                putc(' ', stdout);
            }
        }
        printf("\n");
    }
    for (int xpos = 0; xpos < BARCOUNT; ++xpos) {
        putc('-', stdout);
    }
    printf("\n");
}

sl_result capture_and_display(ILidarDriver * lidar)
{
    sl_result ans;
    
	sl_lidar_response_measurement_node_hq_t nodes[8192];
    size_t   count = _countof(nodes);

    printf("waiting for data...\n");

    ans = lidar->grabScanDataHq(nodes, count, 0);
    if (SL_IS_OK(ans) || ans == SL_RESULT_OPERATION_TIMEOUT) {
        lidar->ascendScanData(nodes, count);
        plot_histogram(nodes, count);

        printf("Do you want to see all the data? (y/n) ");
        int key = getchar();
        if (key == 'Y' || key == 'y') {
            for (int pos = 0; pos < (int)count ; ++pos) {
				printf("%s theta: %03.2f Dist: %08.2f \n", 
                    (nodes[pos].flag & SL_LIDAR_RESP_HQ_FLAG_SYNCBIT) ?"S ":"  ", 
                    (nodes[pos].angle_z_q14 * 90.f) / 16384.f,
                    nodes[pos].dist_mm_q2/4.0f);
            }
        }
    } else {
        printf("error code: %x\n", ans);
    }

    return ans;
}

int main(int argc, const char * argv[]) {
	const char *opt_channel = NULL;
    const char *opt_channel_param_first = NULL;
    sl_u32      opt_channel_param_second = 0;
    sl_result   op_result;
	int         opt_channel_type = CHANNEL_TYPE_SERIALPORT;

    IChannel* _channel;

    if (argc < 5) {
        print_usage(argc, argv);
        return -1;
    }

	const char * opt_is_channel = argv[1];
	if(strcmp(opt_is_channel, "--channel")==0) // opt_is_channel == "--channel"
	{
		opt_channel = argv[2];
		if(strcmp(opt_channel, "-s")==0 || strcmp(opt_channel, "--serial")==0) // modalita' serial
		{
			opt_channel_param_first = argv[3]; // port
			if (argc>4) opt_channel_param_second = strtoul(argv[4], NULL, 10); // baudrate
		}
		else if(strcmp(opt_channel, "-u")==0||strcmp(opt_channel, "--udp")==0) // altra modalita' non importante
		{
			opt_channel_param_first = argv[3];
			if (argc>4) opt_channel_param_second = strtoul(argv[4], NULL, 10);
			opt_channel_type = CHANNEL_TYPE_UDP;
		}
		else // errore: print usage
		{
			print_usage(argc, argv);
			return -1;
		}
	}
    else // il primo argomento DEVE essere "--channel"
	{
		print_usage(argc, argv);
		return -1;
	}

    // create the driver instance
	ILidarDriver * lidar = *createLidarDriver();

    if (!lidar) {
        fprintf(stderr, "insufficent memory, exit\n");
        exit(-2);
    }

    do {
        // try to connect
        if (opt_channel_type == CHANNEL_TYPE_SERIALPORT) { // connessione seriale
            _channel = (*createSerialPortChannel(opt_channel_param_first, opt_channel_param_second));
        }
        else if (opt_channel_type == CHANNEL_TYPE_UDP) { // altra connessione
            _channel = *createUdpChannel(opt_channel_param_first, opt_channel_param_second);
        }
        
        if (SL_IS_FAIL((lidar)->connect(_channel))) {
			switch (opt_channel_type) {	
				case CHANNEL_TYPE_SERIALPORT:
					fprintf(stderr, "Error, cannot bind to the specified serial port %s.\n"
						, opt_channel_param_first);
					break;
				case CHANNEL_TYPE_UDP:
					fprintf(stderr, "Error, cannot connect to the ip addr %s with the udp port %u.\n"
						, opt_channel_param_first, opt_channel_param_second);
					break;
			}
        }

        // retrieving the device info
        ////////////////////////////////////////

        // calcola info varie
        sl_lidar_response_device_health_t healthinfo;
        sl_lidar_response_device_info_t devinfo;

        op_result = lidar->getDeviceInfo(devinfo);

        if (SL_IS_FAIL(op_result)) {
            if (op_result == SL_RESULT_OPERATION_TIMEOUT) {
                // you can check the detailed failure reason
                fprintf(stderr, "Error, operation time out.\n");
            } else {
                fprintf(stderr, "Error, unexpected error, code: %x\n", op_result);
                // other unexpected result
            }
            break;
        }

        // print out the device serial number, firmware and hardware version number..
        printf("SLAMTEC LIDAR S/N: ");
        for (int pos = 0; pos < 16 ;++pos) {
            printf("%02X", devinfo.serialnum[pos]);
        }

        printf("\n"
                "Version:  %s \n"
                "Firmware Ver: %d.%02d\n"
                "Hardware Rev: %d\n"
                , "SL_LIDAR_SDK_VERSION"
                , devinfo.firmware_version>>8
                , devinfo.firmware_version & 0xFF
                , (int)devinfo.hardware_version);


        // check the device health
        ////////////////////////////////////////
        op_result = lidar->getHealth(healthinfo);
        if (SL_IS_OK(op_result)) { // the macro IS_OK is the preperred way to judge whether the operation is succeed.
            printf("Lidar health status : ");
            switch (healthinfo.status) 
			{
				case SL_LIDAR_STATUS_OK:
					printf("OK.");
					break;
				case SL_LIDAR_STATUS_WARNING:
					printf("Warning.");
					break;
				case SL_LIDAR_STATUS_ERROR:
					printf("Error.");
					break;
            }
            printf(" (errorcode: %d)\n", healthinfo.error_code);

        } else {
            fprintf(stderr, "Error, cannot retrieve the lidar health code: %x\n", op_result);
            break;
        }


        if (healthinfo.status == SL_LIDAR_STATUS_ERROR) {
            fprintf(stderr, "Error, slamtec lidar internal error detected. Please reboot the device to retry.\n");
            // enable the following code if you want slamtec lidar to be reboot by software
            // lidar->reset();
            break;
        }

		switch (opt_channel_type) 
		{	
			case CHANNEL_TYPE_SERIALPORT:
				lidar->setMotorSpeed();
			break;
		}

        // take only one 360 deg scan and display the result as a histogram
        ////////////////////////////////////////////////////////////////////////////////
        if (SL_IS_FAIL(lidar->startScan( 0,1 ))) // you can force slamtec lidar to perform scan operation regardless whether the motor is rotating
        {
            fprintf(stderr, "Error, cannot start the scan operation.\n");
            break;
        }

		delay(3000);

        if (SL_IS_FAIL(capture_and_display(lidar))) {
            fprintf(stderr, "Error, cannot grab scan data.\n");
            break;

        }

    } while(0);

    lidar->stop();
    switch (opt_channel_type) 
	{	
		case CHANNEL_TYPE_SERIALPORT:
			delay(20);
			lidar->setMotorSpeed(0);
		break;
	}
    if(lidar) {
        delete lidar;
        lidar = NULL;
    }
    return 0;
}
