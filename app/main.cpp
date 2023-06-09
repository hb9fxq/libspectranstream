#include <iostream>
#include "spectranstream.h"

#include <chrono>
#include <thread>


#include <fstream>

//this tool is only for Frank's debug purpose....
int main()
{

   
    spectran_stream streamer(spectran_stream::STREAMER_TYPE::QUEUED_CF32,"127.0.0.1:54665");

    int readsize = 16000;
    unsigned char appBuffer[readsize*2*sizeof(float)];
    streamer.UpdateDemodulator("Block_IQDemodulator_1", "Block_Spectran_V6B_1", 103e6,-0e6, 3e6);
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
