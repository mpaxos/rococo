#include "all.h"

namespace deptran {
  char RW_BENCHMARK_TABLE[] = "history";

  void RWPiece::reg_all() {
      reg_pieces();
  }

  void RWPiece::reg_pieces() {
    SHARD_PIE(RW_BENCHMARK_R_TXN, RW_BENCHMARK_R_TXN_0, TPCC_TB_HISTORY, TPCC_VAR_H_KEY)
    BEGIN_PIE(RW_BENCHMARK_R_TXN, RW_BENCHMARK_R_TXN_0, DF_NO) {
      mdb::MultiBlob buf(1);
      Value result;
      verify(cmd.input.size()==1);
      auto id = cmd.input[0].get_i64();
      buf[0] = cmd.input[0].get_blob();
      auto row = dtxn->Query(dtxn->GetTable(RW_BENCHMARK_TABLE), buf);
      if (!dtxn->ReadColumn(row, 1, &result)) {
          *res = REJECT;
          return;
      }
      output[0] = result;
      *res = SUCCESS;
      return;
    } END_PIE

    SHARD_PIE(RW_BENCHMARK_W_TXN, RW_BENCHMARK_W_TXN_0, TPCC_TB_HISTORY, TPCC_VAR_H_KEY)
    BEGIN_PIE(RW_BENCHMARK_W_TXN, RW_BENCHMARK_W_TXN_0, DF_REAL) {
      mdb::MultiBlob buf(1);
      Value result;
      verify(cmd.input.size() == 1);
      auto id = cmd.input[0].get_i64();
      buf[0] = cmd.input[0].get_blob();
      auto row = dtxn->Query(dtxn->GetTable(RW_BENCHMARK_TABLE), buf);
      if (!dtxn->ReadColumn(row, 1, &result, TXN_DEFERRED)) {
        *res = REJECT;
        return;
      }
      result.set_i32(result.get_i32()+1);
      if (!dtxn->WriteColumn(row, 1, result, TXN_DEFERRED)) {
        *res = REJECT;
        return;
      }
      *res = SUCCESS;
      return;
    } END_PIE
  }
}

