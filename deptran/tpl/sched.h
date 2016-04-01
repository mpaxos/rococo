//
// Created by shuai on 11/25/15.
//

#pragma once

#include "../classic/sched.h"

namespace rococo {

class Executor;
class TPLSched: public ClassicSched {
 public:
  TPLSched();

  virtual int OnPhaseOneRequest(const SimpleCommand& cmd,
                                rrr::i32 *res,
                                map<int32_t, Value> *output,
                                rrr::DeferredReply *defer);

  virtual mdb::Txn *get_mdb_txn(const i64 tid);
//  virtual mdb::Txn *get_mdb_txn(const RequestHeader &req);
  virtual mdb::Txn *del_mdb_txn(const i64 tid);

};

} // namespace rococo