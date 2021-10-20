#include <iostream>
#include "spectranstream.h"

#include <fstream>

//this tool is only for Frank's debug purpose....
int main()
{

    spectran_stream streamer(spectran_stream::STREAMER_TYPE::QUEUED_CF32,"192.168.178.178:54664");

    int readsize = 16000;
    std::complex<float> appBuffer[readsize];
    streamer.UpdateDemodulator(101e6,-4e6, 12e6);
    streamer.StartStreamingThread();
    //std::ofstream out("/home/f102/debug2.iq", std::ios::out | std::ios::binary);

    

    while (true)
    {

        streamer.GetSamples(readsize, appBuffer);
       //out.write((char *) &appBuffer, sizeof(appBuffer));
    }

    return 0;
}
