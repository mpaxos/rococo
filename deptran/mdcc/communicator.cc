//
// Created by lamont on 12/22/15.
//

#include "communicator.h"
#include "option.h"
#include "deptran/multi_value.h"
#include "MdccScheduler.h"

namespace mdcc {
  // initiate a mdcc transaction
  void MdccCommunicator::SendStart(StartRequest& req,
                                   Callback<StartResponse>& callback) {
    std::lock_guard<std::mutex> lock(this->mtx_);
    auto local_sites = config_->SitesByLocaleId(site_info_.locale_id);
    auto proxy = (local_sites.size()>0) ? RandomSiteProxy(local_sites) : RandomSiteProxy(config_->sites_);

    Log::info("%s txn %ld from site %d to site %d", __FUNCTION__, req.txn_id, site_info_.id, proxy->site_info.id);

    rrr::FutureAttr future;
    future.callback = [this, req, proxy, callback] (Future *fu) {
      StartResponse res;
      Log_debug("SendStart callback for %ld from %ld", req.txn_id, proxy->site_info.id);
      auto& marshall = fu->get_reply();
      marshall >> res;
      callback(res);
    };

    Future::safe_release(proxy->client->async_Start(req, future));
  }

  // start a transaction piece on a partition
  void MdccCommunicator::SendStartPiece(const rococo::SimpleCommand& cmd,
                                        Callback<StartPieceResponse>& callback) {
    std::lock_guard<std::mutex> lock(this->mtx_);
    auto& site = config_->SiteById(cmd.GetParId());
    auto proxy = ClosestSiteProxy(site.partition_id_);

    rrr::FutureAttr future;
    future.callback = [cmd, callback, proxy] (Future *fu) {
      StartPieceResponse res;
      Log_debug("SendStartPiece callback for %ld from %ld", cmd.id_, proxy->site_info.id);
      auto& marshall = fu->get_reply();
      marshall >> res;
      callback(res);
    };

    Future::safe_release(proxy->client->async_StartPiece(cmd, future));
  }

  void MdccCommunicator::SendProposal(BallotType ballotType, txnid_t txn_id,
                                      const rococo::SimpleCommand &cmd,
                                      OptionSet* options,
                                      Callback<OptionSet>& callback) {
    auto partition_id = config_->SiteById(cmd.GetParId()).partition_id_;
    auto partition_sites = config_->SitesByPartitionId(partition_id);

    if (ballotType == BallotType::CLASSIC) {
      auto proxy = LeaderSiteProxy(options, partition_sites);
      Log_debug("send %d options to site %d", options->Options().size(), proxy->site_info.id);

      ProposeRequest req;
      req.updates = *options;
      rrr::FutureAttr future;
      future.callback = [options, callback](rrr::Future* future) {
        auto& m = future->get_reply();
        ProposeResponse res;
        m >> res;
        if (res.accepted) options->Accept();
        callback(*options);
      };
      Future::safe_release(proxy->leader->async_Propose(req, future));
    } else {
      Log_fatal("implement fast path");
    }
  }

  void MdccCommunicator::SendPhase2a(Phase2aRequest req) {
    Log_debug("Site %d: %s", this->site_info_.id, __FUNCTION__);
    assert(req.site_id == site_info_.id);
    auto partition_id = site_info_.partition_id_;
    auto proxies = AllSiteProxies(partition_id);
    for (auto proxy : proxies) {
      Future::safe_release(proxy->acceptor->async_Phase2a(req));
    }
  }

  MdccCommunicator::SiteProxy* MdccCommunicator::ClosestSiteProxy(uint32_t partition_id) const {
    auto partition_sites = config_->SitesByPartitionId(partition_id);
    assert(partition_sites.size()>0);
    auto it = find_if(partition_sites.begin(), partition_sites.end(),
                      [this](const Config::SiteInfo& si) {return si.locale_id==this->site_info_.locale_id;});
    verify(it != partition_sites.end());
    auto site_id = (*it).id;
    assert(site_id >= 0 && site_id < site_proxies_.size());
    return site_proxies_[site_id];
  }

  std::vector<MdccCommunicator::SiteProxy*> MdccCommunicator::AllSiteProxies(parid_t partition_id) const {
    auto partition_sites = config_->SitesByPartitionId(partition_id);
    std::vector<MdccCommunicator::SiteProxy*> results(partition_sites.size());
    assert(partition_sites.size()>0);
    for (int i=0; i<partition_sites.size(); i++) {
      results[i] = site_proxies_[partition_sites[i].id];
    }
    return results;
  }

  MdccCommunicator::SiteProxy* MdccCommunicator::RandomSiteProxy(const vector<Config::SiteInfo> &sites) const {
    return site_proxies_[sites[RandomGenerator::rand(0, sites.size()-1)].id];
  }

  MdccCommunicator::SiteProxy* MdccCommunicator::LeaderSiteProxy(
      OptionSet *option_set, std::vector<Config::SiteInfo> &sites) const {
    std::size_t hname = std::hash<std::string>()(option_set->Table());
    std::size_t hkey = rococo::multi_value_hasher()(option_set->Key());
    int index = (hname ^ hkey) % sites.size();
    return site_proxies_[sites[index].id];
  }
}
