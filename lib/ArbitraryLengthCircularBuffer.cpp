#include "ArbitraryLengthCircularBuffer.h"
#include <cstring>

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
    std::memcpy(data_ + m_end_index, data, bytes_to_write);
    m_end_index += bytes_to_write;
    if (m_end_index == m_capacity) m_end_index = 0;
  }
  else // wrap
  {
    size_t size_block_1 = m_capacity - m_end_index;
    std::memcpy(data_ + m_end_index, data, size_block_1);
    size_t size_block_2 = bytes_to_write - size_block_1;
    std::memcpy(data_, data + size_block_1, size_block_2);
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
    std::memcpy(data, data_ + m_begin_index, bytes_to_read);
    m_begin_index += bytes_to_read;
    if (m_begin_index == m_capacity) m_begin_index = 0;
  }
  else // wrap
  {
    size_t size_block_1 = m_capacity - m_begin_index;
    std::memcpy(data, data_ + m_begin_index, size_block_1);
    size_t size_block_2 = bytes_to_read - size_block_1;
    std::memcpy(data + size_block_1, data_, size_block_2);
    m_begin_index = size_block_2;
  }

  m_size -= bytes_to_read;
}