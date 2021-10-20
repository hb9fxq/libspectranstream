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

#include "spectranstream.h"

spectran_stream::spectran_stream(STREAMER_TYPE steramerType, const std::string &endpoint)
    : m_endpoint(endpoint), m_streamertype(steramerType)
{
    m_buffer = new unsigned char[1];
}

spectran_stream::~spectran_stream()
{
    m_stop_stream_flag = true;
    curl_easy_cleanup(m_curl);
}

void spectran_stream::notify_json_preamble_ready()
{
    std::string s(m_jsonData.begin(), m_jsonData.end());
    m_json_preamble_doc.Parse(s.c_str());
    m_samples_in_next_buffer = m_json_preamble_doc["samples"].GetInt(); //contains number of complex samples in package
    m_remaining_sample_data_bytes = m_samples_in_next_buffer * 2 * sizeof(float);

    // clear previous buffer
    if (m_buffer)
    {
        delete[] m_buffer;
    }

    // make room for following sample data.
    m_buffer = new unsigned char[m_samples_in_next_buffer * 2 * sizeof(float)];
}

void spectran_stream::notify_sample_data_chunk_ready()
{
}

int spectran_stream::GetSamples(int numOfSampels, std::complex<float> *buf)
{
    m_pending_samples_read = numOfSampels;
    std::unique_lock<std::mutex> lock(m_queueMutex);

    //clamping to sample buffer size
    //int fetchSize = numOfSampels;
    //if(m_sample_vector.size()<=numOfSampels){
    //    fetchSize = m_sample_vector.size();
    //}

    // wait till requested number of samples can be provided
    m_queue_cond.wait(lock, [this]()
                      { return this->m_sample_vector.size() > this->m_pending_samples_read * 2; });

    lock.unlock(); // since we start from position0 this is not a critical situation, till we'll erase them...
    std::memcpy(buf, &m_sample_vector[0], numOfSampels * 2 * sizeof(float));

    lock.lock(); // remove consumed samples from vector
    m_sample_vector.erase(m_sample_vector.begin(), m_sample_vector.begin() + numOfSampels);
    lock.unlock();

    return numOfSampels;
}

int spectran_stream::pull_samples_from_write_buffer(char *buffer, int capacity)
{
    int remaining_bytes = 0;

    if (m_remaining_sample_data_bytes > 0)
    {

        // clamp bytes to take from the buffer to capacity or remaining bytes to be processed
        if (m_remaining_sample_data_bytes > capacity)
        {
            remaining_bytes = capacity;
        }
        else
        {
            remaining_bytes = m_remaining_sample_data_bytes;
        }

        memcpy(m_buffer + m_buffer_offset, buffer, remaining_bytes);
        m_buffer_offset += remaining_bytes;
        m_remaining_sample_data_bytes -= remaining_bytes;

        if (m_remaining_sample_data_bytes == 0) // captured a complete buffer
        {
            m_buffer_offset = 0;
            auto *samples = reinterpret_cast<std::complex<float> *>(m_buffer);

            m_queueMutex.lock();

            //TODO: (WTF) We have to maintain a local buffer with copies, because GetSamples() and RTSA streaming is
            //      async.... maybe a smart person will find a smarter way
           
            //m_sample_vector.reserve(capacity);
            std::copy(samples, samples + m_samples_in_next_buffer, std::back_inserter(m_sample_vector));
            
            m_queueMutex.unlock();

            if (m_pending_samples_read > 0 && m_sample_vector.size() >= m_pending_samples_read * 2)
            {
                m_queue_cond.notify_one();
            }
        }
    }

    return capacity - remaining_bytes;
}

size_t spectran_stream::http_data_write_func(char *buffer, size_t size, size_t nmemb, void *userp)
{

    spectran_stream *parent = static_cast<spectran_stream *>(userp);

    int realsize = size * nmemb;

    //fist round will pull all pending samples, if any
    int remainingBytes = parent->pull_samples_from_write_buffer(buffer, realsize);

    //byte by byte mode to isolate json preamble.
    int readBufferoffset = realsize - remainingBytes;
    int readBufferTake = 0;
    for (int i = readBufferoffset; i < realsize; i++)
    {
        remainingBytes--;

        //captured a full preamble!
        if (buffer[i] == '\x1e')
        {
            std::copy(buffer + readBufferoffset, buffer + readBufferoffset + readBufferTake, std::back_inserter(parent->m_jsonData));
            parent->notify_json_preamble_ready();
            parent->m_jsonData.clear();
            break;
        }
        else
        {
            //parent->m_jsonData.push_back(buffer[i]);
            readBufferTake++;
        }
    }

    // treat the rest of the data as sample data
    if (remainingBytes > 0)
    {
        parent->pull_samples_from_write_buffer(buffer + (realsize - remainingBytes), remainingBytes);
    }

    return realsize;
}

void spectran_stream::init_and_start_http_client(std::string const &endpoint)
{

    m_curl = curl_easy_init();
    if (m_curl)
    {
        curl_easy_setopt(m_curl, CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, spectran_stream::http_data_write_func);
        curl_easy_setopt(m_curl, CURLOPT_HTTP_CONTENT_DECODING, 0L);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, static_cast<void *>(this));

        curl_easy_perform(m_curl);

        curl_easy_cleanup(m_curl);
    }
    return;
}

void spectran_stream::StartStreamingThread()
{
    std::thread(&spectran_stream::init_and_start_http_client, this, "http://" + m_endpoint + "/stream?format=float32").detach();
}

size_t write_data_null_sink(void *buffer, size_t size, size_t nmemb, void *userp)
{
    return size * nmemb;
}


void spectran_stream::put_remoteconfig(std::string const &json){
    
    const std::string config_endpoint("http://" + m_endpoint + "/remoteconfig");

    auto pcurl = curl_easy_init();
    if (pcurl)
    {

        //std::cout << req_spectran << "\n";
        curl_easy_setopt(pcurl, CURLOPT_URL, config_endpoint.c_str());
        curl_easy_setopt(pcurl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(pcurl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(pcurl, CURLOPT_WRITEFUNCTION, write_data_null_sink);
        curl_easy_perform(pcurl);
        curl_easy_cleanup(pcurl);
    }
}

void spectran_stream::UpdateDemodulator(float center_frequency, float center_offset, float samp_rate)
{
    //TODO: figure out how to updated config in one run....
    std::string req_iq = m_config_req_string_iqdemod;
    req_iq.replace(req_iq.find("$0"), 2, std::to_string(m_config_req_counter++));
    req_iq.replace(req_iq.find("$1"), 2, std::to_string((long)center_frequency));
    req_iq.replace(req_iq.find("$2"), 2, std::to_string((long)samp_rate));
    req_iq.replace(req_iq.find("$3"), 2, std::to_string((long)samp_rate));
    put_remoteconfig(req_iq);

    std::string req_spectran = m_config_req_string_spectran;
    req_spectran.replace(req_spectran.find("$0"), 2, std::to_string(m_config_req_counter++));
    req_spectran.replace(req_spectran.find("$1"), 2, std::to_string((long)(center_frequency + center_offset)));
    put_remoteconfig(req_spectran);
}