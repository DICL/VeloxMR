#pragma once
#include "message.hh"

namespace eclipse {
namespace messages {

struct SubJobStatus: public Message {
  std::string get_type() const override;

  uint32_t task_id = 0;
  bool is_success = false;
};

}
}
