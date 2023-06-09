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

#include "ArbitraryLengthCircularBuffer.h"


spectran_stream::spectran_stream(STREAMER_TYPE streamerType, const std::string &endpoint)
    : m_endpoint(endpoint), m_streamertype(streamerType)
{
    m_buffer = new unsigned char[1];
    buff = new ArbitraryLengthCircularBuffer(8 * 1024 * 1024); // 8 MB Ringbuffer

    switch (streamerType)
    {
    case STREAMER_TYPE::QUEUED_CF32:
        m_atomic_sample_dtype_size = sizeof(float);
        m_streaming_endpoint_url = "/stream?format=float32";
        break;
    case STREAMER_TYPE::QUEUED_INT16:
        m_atomic_sample_dtype_size = sizeof(short);
        m_streaming_endpoint_url = "/stream?format=int16&scale=32767";
        break;

    default:
        break;
    }
}

spectran_stream::~spectran_stream()
{
    m_stop_stream_flag = true;
    curl_easy_cleanup(m_curl);
}

void spectran_stream::notify_json_preamble_ready(int length)
{
    std::string s(m_jsonData, m_jsonData + length);
    m_json_preamble_doc.Parse(s.c_str());
    m_samples_in_next_buffer = m_json_preamble_doc["samples"].GetInt(); //contains number of complex samples in package
    m_remaining_sample_data_bytes = m_samples_in_next_buffer * 2 * m_atomic_sample_dtype_size;

    // clear previous buffer
    if (m_buffer)
    {
        delete[] m_buffer;
    }

    // make room for following sample data.
    m_buffer = new unsigned char[m_samples_in_next_buffer * 2 * m_atomic_sample_dtype_size];
}

int spectran_stream::GetSamples(int numOfSampels, unsigned char *buf)
{
    m_pending_samples_read = numOfSampels;
    std::unique_lock<std::mutex> lock(m_queueMutex);

    // wait till requested number of samples can be provided
    m_queue_cond.wait(lock, [this]()
                      { return buff->size() > this->m_pending_samples_read * 2 * m_atomic_sample_dtype_size; });

    buff->read(buf, m_pending_samples_read * 2 * m_atomic_sample_dtype_size);
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

            m_queueMutex.lock();
            buff->write(m_buffer, m_samples_in_next_buffer * 2 * m_atomic_sample_dtype_size);
            m_queueMutex.unlock();

            if (m_pending_samples_read > 0 && buff->size() / (2 * m_atomic_sample_dtype_size) >= m_pending_samples_read)
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

    // we asume, a json preamble fits into a single http response chunk and is not splitted into two chunks...
    for (int i = readBufferoffset; i < realsize; i++)
    {
        remainingBytes--;

        // captured a full preamble!
        if (buffer[i] == '\x1e')
        {

            parent->m_jsonData = new unsigned char[readBufferTake];
            memcpy(&parent->m_jsonData[0], &buffer[readBufferoffset], readBufferTake);

            parent->notify_json_preamble_ready(readBufferTake);
            delete[] parent->m_jsonData;
            break;
        }
        else
        {
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
    if (!Probe())
    {
        throw std::runtime_error("Could not probe spectran HTTP Server " + m_endpoint);
    }

    std::thread(&spectran_stream::init_and_start_http_client, this, "http://" + m_endpoint + m_streaming_endpoint_url).detach();
}

size_t write_data_null_sink(void *buffer, size_t size, size_t nmemb, void *userp)
{
    return size * nmemb;
}

void spectran_stream::put_remoteconfig(std::string const &json)
{
    const std::string config_endpoint("http://" + m_endpoint + "/remoteconfig");

    auto pcurl = curl_easy_init();
    if (pcurl)
    {
        //std::cout << json << "\n";
        curl_easy_setopt(pcurl, CURLOPT_URL, config_endpoint.c_str());
        curl_easy_setopt(pcurl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(pcurl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(pcurl, CURLOPT_WRITEFUNCTION, write_data_null_sink);
        curl_easy_perform(pcurl);
        curl_easy_cleanup(pcurl);
    }
}

size_t curlStringWriter(void *ptr, size_t size, size_t nmemb, std::string *data)
{
    data->append((char *)ptr, size * nmemb);
    return size * nmemb;
}

bool spectran_stream::Probe()
{

    const std::string infoURL = "http://" + m_endpoint + "/info";

    std::string response_string;
    std::string header_string;

    auto curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, infoURL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlStringWriter);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl = NULL;
    }

    return response_string.find("HTTPServer") != std::string::npos; //parse response and impl. better probe()
}

void spectran_stream::UpdateDemodulator(std::string demod_block, std::string spectran_block, float center_frequency, float center_offset, float samp_rate)
{
    std::string req_iq = m_config_req_string_iqdemod;
    req_iq.replace(req_iq.find("$0"), 2, std::to_string(m_config_req_counter++));
    req_iq.replace(req_iq.find("$1"), 2, std::to_string((long)center_frequency));
    req_iq.replace(req_iq.find("$2"), 2, std::to_string((long)samp_rate));
    req_iq.replace(req_iq.find("$3"), 2, std::to_string((long)samp_rate));
    req_iq.replace(req_iq.find("$4"), 2, demod_block);
    put_remoteconfig(req_iq);

    if(!spectran_block.empty())
    {
        std::string req_spectran = m_config_req_string_spectran;
        req_spectran.replace(req_spectran.find("$0"), 2, std::to_string(m_config_req_counter++));
        req_spectran.replace(req_spectran.find("$1"), 2, std::to_string((long)(center_frequency + center_offset)));
        req_spectran.replace(req_spectran.find("$2"), 2, spectran_block);
        put_remoteconfig(req_spectran);
    }
}