#include <iostream>
#include "spectranstream.h"

#include <chrono>
#include <thread>


#include <fstream>

//this tool is only for Frank's debug purpose....
int main()
{

   
    spectran_stream streamer(spectran_stream::STREAMER_TYPE::QUEUED_CF32,"192.168.178.51:54664");

    int readsize = 16000;
    unsigned char appBuffer[readsize*2*sizeof(float)];
    streamer.UpdateDemodulator(101e6,-4e6, 14e6);
    streamer.StartStreamingThread();
    //std::ofstream out("/home/f102/debug2.iq", std::ios::out | std::ios::binary);

    while (true)
    {

        streamer.GetSamples(readsize, appBuffer);
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //out.write((char *) &appBuffer, readsize*2*sizeof(float));
    }

    return 0;
}
