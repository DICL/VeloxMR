#ifndef ECLIPSEMR_NODES_REMOTEMR_H_
#define ECLIPSEMR_NODES_REMOTEMR_H_
#include "../../nodes/remotedfs.hh"
#include "../../messages/message.hh"
#include "peermr.h"

namespace eclipse {

class RemoteMR: public RemoteDFS {
 public: 
  RemoteMR();

  bool establish();
  void insert_idata(messages::Message *msg);
  void insert_igroup(messages::Message *msg);
  void insert_iblock(messages::Message *msg);
  void request_idata(messages::Message *msg);
  void request_igroup(messages::Message *msg);
  void request_iblock(messages::Message *msg);
  void shuffle(messages::Message *msg);

 protected:
  PeerMR* peer;
};

}  // namespace eclipse
#endif  // ECLIPSEMR_NODES_REMOTEMR_H_
