#include "iwriter.h"
#include <string>
#include <list>
#include <map>
#include <memory>
#include <iterator>
#include <fstream>
#include <utility>
#include <boost/asio.hpp>
#include <thread>
#include "../fs/directory.hh"

using std::list;
using std::vector;
using std::multimap;
using std::string;

// ###########################
// Rule for kmv_blocks_
// front     : Stacking block.
// front + n : Ready to write.
// ###########################
// Rule for naming iblocks
// .job_[job_id]_[map_id]_[reducer_id]_[block_seq]

namespace eclipse {

IWriter::IWriter() {
  reduce_slot_ = con.settings.get<int>("mapreduce.reduce_slot");
  iblock_size_ = con.settings.get<int>("mapreduce.iblock_size");
  scratch_path_ = con.settings.get<string>("path.scratch");
  is_write_start_ = false;
  is_write_finish_ = false;
  index_counter_ = 0;
  writing_index_ = -1;
  write_buf_size_ = con.settings.get<int>("mapreduce.write_buf_size");
  written_bytes_ = 0;
  write_buf_ = (char*)malloc(write_buf_size_ + 1);
  write_pos_ = write_buf_;
  kmv_blocks_.resize(reduce_slot_);
  is_write_ready_.resize(reduce_slot_);
  for (uint32_t i = 0; i < reduce_slot_; ++i) {
    // kmv_blocks_[i].push_front(std::make_shared<multimap<string, string>>());
    block_size_.emplace_back(0);
    write_count_.emplace_back(0);
  }
  writer_thread = std::make_unique<std::thread>(run, this);
}
IWriter::IWriter(const uint32_t job_id, const uint32_t net_id) : IWriter() {
  job_id_ = job_id;
  net_id_ = net_id;
}
void IWriter::set_job_id(const uint32_t job_id) {
  job_id_ = job_id;
}
void IWriter::set_net_id(const uint32_t net_id) {
  net_id_ = net_id;
}
bool IWriter::is_write_finish() {
  return is_write_finish_;
}
IWriter::~IWriter() {
  delete[] write_buf_;
}
void IWriter::finalize() {
  // It should be garuanteed no more key values added
  for (uint32_t i = 0; i < reduce_slot_; ++i) {
    if (kmv_blocks_[i].size() > 0) {
      is_write_ready_[i].front() = true;
    }
  }
  is_write_start_ = true;
  writer_thread->join();
}
void IWriter::run(IWriter *obj) {
  obj->seek_writable_block();
}
void IWriter::seek_writable_block() {
  // while loops should be changed to lock
  while(!is_write_start_);
  while(!is_write_finish_) {
    // Check if there is any block that should be written to disk.
    // And if it's true, write it onto disk.
    for (uint32_t i = 0; i < reduce_slot_; ++i) {
      if (kmv_blocks_[i].size() > 0 && is_write_ready_[i].back()) {
        auto writing_block = kmv_blocks_[i].back();
        kmv_blocks_[i].pop_back();
        is_write_ready_[i].pop_back();
        write_block(writing_block, i);
      }
    }

    // Check if there are no more incoming key value pairs.
    if(is_write_start_) {
      uint32_t finish_counter = 0;
      for (uint32_t i = 0; i < reduce_slot_; ++i) {
        if(kmv_blocks_[i].size() == 0) {
          ++finish_counter;
        }
      }
      if (finish_counter == reduce_slot_) {
        is_write_finish_ = true;
      }
    }
  }
}
void IWriter::add_key_value(const string &key, const string &value) {
  int index;
  index = get_index(key);
  if (kmv_blocks_[index].size() == 0 || is_write_ready_[index].front()) {
    add_block(index);
  }
  auto block = kmv_blocks_[index].front();

  uint32_t new_size;
  if (block->find(key) == block->end()) {
    new_size = get_block_size(index) + key.length() + value.length() + 4;
  } else {
    new_size = get_block_size(index) + value.length() + 1;
  }
  block->emplace(key, value);
  set_block_size(index, new_size);

  if (new_size > iblock_size_) {
    is_write_ready_[index].front() = true;
    is_write_start_ = true;
  }
}
void IWriter::add_block(const uint32_t index) {
  kmv_blocks_[index].push_front(
      std::make_shared<std::multimap<string, string>>());
  is_write_ready_[index].emplace_front(false);
  block_size_[index] = 0;
}
int IWriter::get_block_size(const uint32_t index) {
  return block_size_[index];
}
void IWriter::set_block_size(const uint32_t index, const uint32_t size) {
  block_size_[index] = size;
}
int IWriter::get_index(const string &key) {
  auto it = key_index_.find(key);
  if (it != key_index_.end()) {
    return it->second;
  } else {
    int index = index_counter_ % reduce_slot_;
    ++index_counter_;
    key_index_.emplace(key, index);
    return index;
  }
}
void IWriter::write_record(const string &record) {
  int record_size = record.length();
  if (written_bytes_ + record_size > write_buf_size_)
    flush_buffer();
  buffer_record(record);
}
void IWriter::buffer_record(const string &record) {
  uint32_t record_size = record.length();
  strncpy(write_pos_, record.c_str(), record_size);
  write_pos_ += record_size;
  strncpy(write_pos_, "\n", 1);
  ++write_pos_;
  written_bytes_ += record_size + 1;
}
void IWriter::flush_buffer() {
  strncpy(write_pos_, "\0", 1);
  file_ << write_buf_;
  write_pos_ = write_buf_;
  written_bytes_ = 0;
}
void IWriter::set_writing_file(const uint32_t index) {
  writing_index_ = index;
  string file_path;
  get_new_path(file_path);
  file_.open(file_path);
}
void IWriter::write_block(std::shared_ptr<std::multimap<string, string>> block,
    const uint32_t index) {
  set_writing_file(index);
  std::pair<multimap<string, string>::iterator,
      multimap<string, string>::iterator> ret;
  for (auto key_it = block->begin(); key_it != block->end(); key_it =
      ret.second) {
    ret = block->equal_range(key_it->first);
    write_record(key_it->first);
    int num_value = std::distance(ret.first, ret.second);
    write_record(std::to_string(num_value));
    for (auto it = ret.first; it != ret.second; ++it) {
      write_record(it->second);
    }
  } 
  flush_buffer();
  file_.close();
  block_size_[index] = 0;
}
void IWriter::get_new_path(string &path) {
  path = scratch_path_ + "/.job_" + std::to_string(job_id_) + "_" +
      std::to_string(net_id_) + "_" + std::to_string(writing_index_) + "_" +
      std::to_string(write_count_[writing_index_]);
  ++write_count_[writing_index_];
}

//void IWriter::flush(const uint32_t index) {
//  auto block = &kmv_blocks_[index];
//  std::ofstream file;
//  string file_path;
//  // TODO(wbkim): Get the path of intermediate data.
//  // file_path = SomeFunctionToGetPath();
//  file.open(file_path);
//
//  // Write into the file.
//  // TODO(wbkim): Should be changed into asynchronous manner.
//  std::pair<multimap<string, string>::iterator,
//      multimap<string, string>::iterator> ret;
//  for (auto key_it = block->begin(); key_it != block->end(); key_it =
//      ret.second) {
//    ret = block->equal_range(key_it->first);
//    file << key_it->first << std::endl;
//    int num_value = std::distance(ret.first, ret.second);
//    file << std::to_string(num_value) << std::endl;
//    for (auto it = ret.first; it != ret.second; ++it) {
//      file << it->second << std::endl;
//    }
//  }
//
//  block->clear();
//  set_block_size(index, 0);
//
//  file.close();
//
//  // TODO(wbkim): Write file information into db.
//  
//}
//// This function should return immediately.
//void IWriter::async_flush(const uint32_t index) {
//  // allocate new block onto block list
//  kmv_blocks_[index].resize(kmv_blocks_[index].size() + 1);
//  auto block = &kmv_blocks_[index].back();
//
//  boost::asio::io_service io_service;
//  boost::asio::const_buffers_1 buf;
//  string file_path;
//  // TODO(wbkim): Get the path of intermediate data.
//  // file_path = SomeFunctionToGetPath();
//  int fd = open(file_path.c_str(), O_WRONLY | O_CREAT, 00664);
//  boost::asio::posix::stream_descriptor sd(io_service);
//  sd.assign(fd);
//
//  std::pair<multimap<string, string>::iterator,
//      multimap<string, string>::iterator> ret;
//  for (auto key_it = block->begin(); key_it != block-end(); ket_it =
//      ret.second) {
//    ret = block->equal_range(key_it->first);
//    boost::asio::const_buffers_1 buf = boost::asio::buffer(
//        key_it->fisrt + '\n');
//    boost::asio::async_write(sd, buf, boost::bind(write_handler,
//        boost::asio::placeholders::error,
//        boost::asio::placeholders::bytes_transferred));
//    int num_value = std::distance(ret.first, ret.second);
//    buf = boost::asio::buffer(std::to_string(num_value) + '\n');
//    boost::asio::async_write(sd, buf, boost::bind(write_handler,
//        boost::asio::placeholders::error,
//        boost::asio::placeholders::bytes_transferred));
//    for (auto it = ret.first; it != ret.second; ++it) {
//      buf = boost::asio::buffer(it->second + '\n');
//      boost::asio::async_write(sd, buf, boost::bind(write_handler,
//          boost::asio::placeholders::error,
//          boost::asio::placeholders::bytes_transferred));
//}
//void IWriter::write_handler(const boost::system::error_code &ec,
//    std::size_t bytes_transferred) {
//}

}  // namespace eclipse
