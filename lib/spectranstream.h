/*
MIT License

Copyright (c) 2021 Frank Werner-Krippendorf

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SPECTRANSTREAM_C_H
#define SPECTRANSTREAM_C_H

#include <curl/curl.h>

#include <rapidjson/document.h>

#include <complex>
#include <fstream>
#include <iostream>
#include <mutex>
#include <atomic>

#include <vector>

#include <thread>
#include <chrono>
#include <condition_variable>

#include "ArbitraryLengthCircularBuffer.h"

#include <string.h>

class spectran_stream
{
public:
    enum STREAMER_TYPE
    {
        RAW_STDOUT_CF32,
        QUEUED_CF32,
        QUEUED_INT16
    };

    spectran_stream(STREAMER_TYPE steramerType, const std::string& endpoint);
    ~spectran_stream();

    int GetSamples(int numOfSampels, unsigned char *buf);
    void StartStreamingThread();
    void UpdateDemodulator(float center_frequency, float center_offset, float samp_rate);
    bool Probe();

private:
    std::string m_endpoint;

    ArbitraryLengthCircularBuffer *buff;
    
    void init_and_start_http_client(std::string const &endpoint);
    static size_t http_data_write_func(char *buffer, size_t size, size_t nmemb, void *userp);
    void notify_json_preamble_ready(int length);
    int pull_samples_from_write_buffer(char *buffer, int capacity);

    void put_remoteconfig(std::string const &json);

    unsigned char *m_buffer;
    int m_buffer_offset = 0;
    int m_samples_in_next_buffer = 0;
    int m_atomic_sample_dtype_size = 0;
    std::string m_streaming_endpoint_url = "";

    unsigned char *m_jsonData;
 
    int m_remaining_sample_data_bytes = 0;
    int m_pending_samples_read = 0;

    rapidjson::Document m_json_preamble_doc;

    STREAMER_TYPE m_streamertype;

    std::mutex m_queueMutex;
    std::condition_variable m_queue_cond;

    bool m_stop_stream_flag = false;
    CURL *m_curl;

    //TODO quick and dirty json templates to update via /remoteconifg
    int m_config_req_counter = 0;
    
    const std::string m_config_req_string_iqdemod = "{\n"
"   \"request\": $0,\n"
"   \"receiverName\": \"Block_IQDemodulator_0\",\n"
"   \"config\": {\n"
"      \"items\": [\n"
"         {\n"
"            \"receiverName\": \"Block_IQDemodulator_0\",\n"
"            \"type\": \"group\",\n"
"            \"name\": \"main\",\n"
"            \"items\": [\n"
"               {\n"
"                  \"type\": \"float\",\n"
"                  \"name\": \"centerfreq\",\n"
"                  \"label\": \"Center Frequency\",\n"
"                  \"value\": $1\n"
"               },\n"
"               {\n"
"                  \"type\": \"float\",\n"
"                  \"name\": \"samplerate\",\n"
"                  \"label\": \"Sample Rate\",\n"
"                  \"value\": $2\n"
"               },\n"
"               {\n"
"                  \"type\": \"float\",\n"
"                  \"name\": \"spanfreq\",\n"
"                  \"label\": \"Span Frequency\",\n"
"                  \"value\": $3\n"
"               }\n"
"            ]\n"
"         }\n"
"      ]\n"
"   }\n"
"}";


  const std::string m_config_req_string_spectran = "{\n"
"    \"request\": $0,\n"
"    \"receiverName\": \"Block_Spectran_V6B_0\",\n"
"    \"config\": {\n"
"       \"items\": [\n"
"          {\n"
"             \"receiverName\": \"Block_Spectran_V6B_0\",\n"
"             \"type\": \"group\",\n"
"             \"name\": \"main\",\n"
"             \"items\": [\n"
"                {\n"
"                   \"type\": \"float\",\n"
"                   \"name\": \"centerfreq\",\n"
"                   \"label\": \"Center Frequency\",\n"
"                   \"value\": $1\n"
"                }\n"
"             ]\n"
"          }\n"
"       ]\n"
"    }\n"
" }";

};

#endif