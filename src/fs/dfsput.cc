#include <iostream>
#include <string.h>
#include <fstream>
#include <string>
#include "../common/hash.hh"
#include "../common/context.hh"
#include "fileinfo.hh"
#include "blockinfo.hh"

using namespace std;
using namespace eclipse;

int main(int argc, char* argv[])
{
  Context con;
  if(argc < 2)
  {
    //con.logger->info("[ERROR]usage: dfsput file_name1 file_name2 ...\n");
    cout << "[ERROR]usage: dfsput file_name1 file_name2 ..." << endl;
    return -1;
  }
  else
  {
    //uint32_t BLOCK_SIZE = con.settings.get<int>("filesystem.buffer");
    uint32_t BLOCK_SIZE = con.settings.get<int>("filesystem.block");
    uint32_t NUM_SERVERS = con.settings.get<vector<string>>("network.nodes").size();
    string path = con.settings.get<string>("path.scratch");
    //    file_info.replica = con.settings.get<int>("filesystem.replica");
    for(int i=1; i<argc; i++)
    {
      char *buff = new char[BLOCK_SIZE];
      bzero(buff, BLOCK_SIZE);
      //char buff[BLOCK_SIZE] = {0};
      ifstream myfile;
      string file_name = argv[i];
      myfile.open(file_name);
      myfile.seekg(0, myfile.end);
      uint64_t file_size = myfile.tellg();
      unsigned int block_seq = 0;
      uint32_t start = 0;
      uint32_t end = start + BLOCK_SIZE - 1;
      uint32_t file_hash_key = h (file_name);

      //TODO: remote_metadata_server = lookup(hkey);
      int remote_metadata_server = 1;

      uint32_t file_id;
      while(1)
      {
        file_id = rand();

        // TODO: if(!remote_metadata_server.is_exist(file_id))
        if(1)

        {
          break;
        }
      }
      FileInfo file_info;
      file_info.file_id = file_id;
      file_info.file_name = file_name;
      file_info.file_hash_key = file_hash_key;
      file_info.file_size = file_size;
      file_info.num_block = block_seq;

      // PSEUDO CODE
      // TODO: remote_metadata_server.insert_file_metadata(file_info);
      //cout << "remote_metadata_server.insert_file_metadata(file_info);" << endl;

      // this function should call FileIO.open_file() in remote metadata server;


      while(1)
      {
        
        if(end < file_size){
          myfile.seekg(start+BLOCK_SIZE-1, myfile.beg);

          while(1)
          {
            if(myfile.peek() =='\n') break;
            else
            {
              myfile.seekg(-1, myfile.cur);
              end--;
            }
          }
          myfile.seekg(start, myfile.beg);
          myfile.read(buff, end-start);
          uint32_t block_size = end - start;
          start = end + 1;
          end = start + BLOCK_SIZE - 1;

          unsigned int block_hash_key = rand()%NUM_SERVERS;

          //TODO: int remote_server = lookup(block_hash_key);
          int remote_server = 1;

          BlockInfo block_info;
          block_info.file_id = file_id;
          block_info.block_seq = block_seq;
          block_info.block_hash_key = block_hash_key;
          block_info.block_name = file_info.file_name + "_" + to_string(block_seq++);
          block_info.block_size = block_size;
          block_info.is_inter = 0;
          //block_info.node = remote_server.ip_address;
          //l_server = lookup((block_hash_key-1+NUM_SERVERS)%NUM_SERVERS);
          //r_server = lookup((block_hash_key+1+NUM_SERVERS)%NUM_SERVERS);
          //block_info.l_node = l_server.ip_address;
          //block_info.r_node = r_server.ip_address;
          block_info.commit = 1;
          file_info.num_block = block_seq;

          //TODO: remote_metadata_server.update_file_metadata(fileinfo.file_id, file_info);
          //cout << "remote_metadata_server.update_file_metadata(fileinfo.file_id, file_info);" << endl;

          //TODO: remote_metadata_server.insert_block_metadata(blockinfo);
          //cout << "remote_metadata_server.insert_block_metadata(blockinfo);" << endl;

          //TODO: remote_server.send_buff(block_info.block_name, buff);
          //cout << "remote_server.send_buff(block_info.block_name, buff);" << endl;
          //remote_server.send_buff(block_hash_key, buff);
          // this function should call FileIO.insert_block(_metadata) in remote metadata server?

          // TODO: remote node part
          ofstream block;
          block.open(path + "/" + block_info.block_name);
          block << buff;
          block.close();
        }
        else{  // last block
          myfile.seekg(start, myfile.beg);
          myfile.read(buff, file_size-start);
          uint32_t block_size = end - start;
          buff[file_size-start-1] = 0;

          uint32_t block_hash_key = rand()%NUM_SERVERS;

          // TODO: remote_server = lookup(block_hash_key);
          //cout << "remote_server = lookup(block_hash_key);" << endl;

          BlockInfo block_info;
          block_info.file_id = file_id;
          block_info.block_seq = block_seq;
          block_info.block_hash_key = block_hash_key;
          block_info.block_name = file_name + "_" + to_string(block_seq++);
          block_info.block_size = block_size;
          block_info.is_inter = 0;
          //block_info.node = remote_server.ip_address;
          //l_server = lookup(block_hash_key-1);
          //r_server = lookup(block_hash_key+1);
          //block_info.l_node = l_server.ip_address;
          //block_info.r_node = r_server.ip_address;
          block_info.commit = 1;
          file_info.num_block = block_seq;

          // TODO: remote_metadata_server.update_file_metadata(fileinfo.file_id, file_info);
          //cout << "remote_metadata_server.update_file_metadata(fileinfo.file_id, file_info);" << endl;

          // TODO: remote_metadata_server.insert_block_metadata(blockinfo);
          //cout << "remote_metadata_server.insert_block_metadata(blockinfo);" << endl;

          // TODO: remote_server.send_buff(block_info.block_name, buff);
          //cout << "remote_server.send_buff(block_info.block_name, buff);" << endl;
          //remote_server.send_buff(block_hash_key, buff);
          // this function should call FileIO.insert_block(_metadata) in remote metadata server;

          // remote node part
          ofstream block;
          block.open(path + "/" + block_info.block_name);
          block << buff;
          block.close();
          break;
        }
      }
      myfile.close();
      delete[] buff;
    }

  }
  return 0;
}