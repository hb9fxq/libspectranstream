/*

ArbitraryLegthCircular buffer is based on https://www.asawicki.info/news_1468_circular_buffer_of_raw_binary_data_in_c 
- The boundary checks have been removed / So read()/write() is enforced.
- Reading / writing within capacity boundaries is managed by the caller
- Code simplified

TODO:
  - Report write overflow (current mode: ignore) callback

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
#ifndef ARBLENGTHBUFFER_C_H
#define ARBLENGTHBUFFER_C_H


#include <algorithm> 

class ArbitraryLengthCircularBuffer
{
public:
  ArbitraryLengthCircularBuffer(size_t capacity);
  ~ArbitraryLengthCircularBuffer();


  size_t size() const { return m_size; }
  size_t capacity() const { return m_capacity; }

  void write(const unsigned char *data, size_t bytes_to_write);
  void read(unsigned char *data, size_t bytes_to_read);

private:
  size_t m_begin_index, m_end_index, m_size, m_capacity;
  unsigned char *data_;
};

ArbitraryLengthCircularBuffer::ArbitraryLengthCircularBuffer(size_t capacity)
  : m_begin_index(0)
  , m_end_index(0)
  , m_size(0)
  , m_capacity(capacity)
{
  data_ = new unsigned char[capacity];
}

ArbitraryLengthCircularBuffer::~ArbitraryLengthCircularBuffer()
{
  delete [] data_;
}

void ArbitraryLengthCircularBuffer::write(const unsigned char *data, size_t bytes_to_write)
{

  if (bytes_to_write == 0) return;

  if (bytes_to_write <= m_capacity - m_end_index)
  {
    memcpy(data_ + m_end_index, data, bytes_to_write);
    m_end_index += bytes_to_write;
    if (m_end_index == m_capacity) m_end_index = 0;
  }
  else // wrap
  {
    size_t size_block_1 = m_capacity - m_end_index;
    memcpy(data_ + m_end_index, data, size_block_1);
    size_t size_block_2 = bytes_to_write - size_block_1;
    memcpy(data_, data + size_block_1, size_block_2);
    m_end_index = size_block_2;
  }


  if(m_size+bytes_to_write >= m_capacity){ //clamp size to capacity
    m_size = m_capacity;
  }
  else{
    m_size += bytes_to_write;
  }
}

void ArbitraryLengthCircularBuffer::read(unsigned char *data, size_t bytes_to_read)
{
  if (bytes_to_read == 0) return;

  if (bytes_to_read <= m_capacity - m_begin_index)
  {
    memcpy(data, data_ + m_begin_index, bytes_to_read);
    m_begin_index += bytes_to_read;
    if (m_begin_index == m_capacity) m_begin_index = 0;
  }
  else // wrap
  {
    size_t size_block_1 = m_capacity - m_begin_index;
    memcpy(data, data_ + m_begin_index, size_block_1);
    size_t size_block_2 = bytes_to_read - size_block_1;
    memcpy(data + size_block_1, data_, size_block_2);
    m_begin_index = size_block_2;
  }

  m_size -= bytes_to_read;
}

#endif
