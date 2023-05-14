#include "raft_server.h"

#include <thread>
#include <algorithm>
#include <bits/stdc++.h>


#include "util.h"

/**
 * @brief Construct a new Raft Server object
 *
 * @param raft_config
 */
RaftServer::RaftServer(RaftConfig raft_config)
    : id_(raft_config.id)
    , role_(NodeRaftRoleEnum::Follower)
    , current_term_(0)
    , voted_for_(-1)
    , commit_idx_(0)
    , last_applied_idx_(0)
    , tick_count_(0)
    , leader_id_(-1)
    , heartbeat_timeout_(1)
    , election_timeout_(0)
    , base_election_timeout_(5)
    , heartbeat_tick_count_(0)
    , election_tick_count_(0)
    , max_entries_per_append_req_(100)
    , tick_interval_(1000) {
  for (auto n : raft_config.peer_address_map) {
    RaftNode* node = new RaftNode(n.first, NodeStateEnum::Init, 0, 0, n.second);
    this->nodes_.push_back(node);
  }
}

EStatus RaftServer::ResetRandomElectionTimeout() {
  // make rand election timeout in (election_timeout, 2 * election_timout)
  auto rand_tick =
      RandomNumber::Between(base_election_timeout_, 2 * base_election_timeout_);
  election_timeout_ = rand_tick;
  return EStatus::kOk;
}

EStatus RaftServer::RunMainLoop(RaftConfig raft_config) {
  RaftServer* svr = new RaftServer(raft_config);
  std::thread th(&RaftServer::RunCycle, svr);
  th.detach();
  return EStatus::kOk;
}

/**
 * @brief Get the Entries To Be Send object
 *
 * @param node
 * @param index
 * @param count
 * @return std::vector<eraftkv::Entry*>
 */
std::vector<eraftkv::Entry*> RaftServer::GetEntriesToBeSend(RaftNode* node,
                                                            int64_t   index,
                                                            int64_t   count) {
  return std::vector<eraftkv::Entry*>{};
}

/**
 * @brief
 *
 * @param term
 * @param vote
 * @return EStatus
 */
EStatus RaftServer::SaveMetaData(int64_t term, int64_t vote) {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::ReadMetaData() {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param id
 * @param is_self
 * @return RaftNode*
 */
RaftNode* RaftServer::JoinNode(int64_t id, bool is_self) {
  return nullptr;
}

/**
 * @brief
 *
 * @param node
 * @return EStatus
 */
EStatus RaftServer::RemoveNode(RaftNode* node) {
  return EStatus::kOk;
}

/**
 * @brief raft core cycle
 *
 * @return EStatus
 */
EStatus RaftServer::RunCycle() {
  ResetRandomElectionTimeout();
  while (true) {
    heartbeat_tick_count_ += 1;
    election_tick_count_ += 1;
    if (heartbeat_tick_count_ == heartbeat_timeout_) {
      TraceLog("DEBUG: ", "heartbeat timeout");
      heartbeat_tick_count_ = 0;
    }
    if (election_tick_count_ == election_timeout_) {
      TraceLog("DEBUG: ", "start election in term", current_term_);
      this->BecomeCandidate();
      this->current_term_ += 1;
      this->ElectionStart(false);
      ResetRandomElectionTimeout();
      election_tick_count_ = 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::SendAppendEntries() {

  // only leader can set append entries
  if (this->role_ != NodeRaftRoleEnum::Leader) {
    return EStatus::kNotSupport;
  }

  for (auto& node : this->nodes_) {
    if (node->id == this->id_) {
      return EStatus::kNotSupport;
    }

    auto prev_log_index = node->next_log_index - 1;

    if (prev_log_index < this->log_store_->FirstIndex()) {
      TraceLog("send snapshot to node: ", node->id);
    } else {
      auto prev_log_entry = this->log_store_->Get(prev_log_index);
      auto copy_cnt = this->log_store_->LastIndex() - prev_log_index;
      if (copy_cnt > this->max_entries_per_append_req_) {
        copy_cnt =  this->max_entries_per_append_req_;
      }

      eraftkv::AppendEntriesReq* append_req = new eraftkv::AppendEntriesReq();
      append_req->set_is_heartbeat(false);   
      if (copy_cnt > 0) {
        auto etys_to_be_send = this->log_store_->Gets(prev_log_index + 1, prev_log_index + copy_cnt);
        for(auto ety: etys_to_be_send) {
            eraftkv::Entry* new_ety = append_req->add_entries();   
            new_ety = ety;
        }
      }
      append_req->set_term(this->current_term_);
      append_req->set_leader_id(this->id_);
      append_req->set_prev_log_index(prev_log_index);
      append_req->set_prev_log_term(prev_log_entry->term());
      append_req->set_leader_commit(this->commit_idx_);

      this->net_->SendAppendEntries(this, node, append_req);

    }
  }

  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::ApplyEntries() {
  return EStatus::kOk;
}

bool RaftServer::IsUpToDate(int64_t last_idx, int64_t term) {
  return last_idx >= this->log_store_->LastIndex() && term >= this->log_store_->GetLastEty()->term();
}

/**
 * @brief
 *
 * @param from_node
 * @param req
 * @param resp
 * @return EStatus
 */
EStatus RaftServer::HandleRequestVoteReq(RaftNode*                 from_node,
                                         eraftkv::RequestVoteReq*  req,
                                         eraftkv::RequestVoteResp* resp) {
  TraceLog("DEBUG: handle vote req ", req->SerializeAsString());
  resp->set_term(this->current_term_);
  resp->set_leader_id(this->leader_id_);

  bool can_vote = (this->voted_for_ == req->candidtate_id()) ||
                  this->voted_for_ == -1 && this->leader_id_ == -1 ||
                  req->term() > this->current_term_;
  
  if (can_vote && this->IsUpToDate(req->last_log_idx(), req->last_log_term())) {
    resp->set_vote_granted(true);
  } else {
    resp->set_vote_granted(false);
    return EStatus::kOk;
  }

  TraceLog("DEBUG: peer ", this->id_, " vote ", req->candidtate_id());
  this->voted_for_ = req->candidtate_id();

  ResetRandomElectionTimeout();
  election_tick_count_ = 0;
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::SendHeartBeat() {
  for (auto node : this->nodes_) {
    if (node->id == this->id_) {
      continue;
    }

    eraftkv::AppendEntriesReq* append_req = new eraftkv::AppendEntriesReq();
    append_req->set_is_heartbeat(true);
    append_req->set_leader_id(this->id_);
    append_req->set_term(this->current_term_);
    append_req->set_leader_commit(this->commit_idx_);

    this->net_->SendAppendEntries(this, node, append_req);

  }

  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param from_node
 * @param resp
 * @return EStatus
 */
EStatus RaftServer::HandleRequestVoteResp(RaftNode*                 from_node,
                                          eraftkv::RequestVoteReq*  req,
                                          eraftkv::RequestVoteResp* resp) {
  if (resp != nullptr) {
    TraceLog("DEBUG: ",
             "send request vote revice resp: ",
             resp->SerializeAsString(),
             " from node ",
             from_node->address);
    if (this->current_term_ == req->term() &&
        this->role_ == NodeRaftRoleEnum::Candidate) {
      if (resp->vote_granted()) {
        this->granted_votes_ += 1;
        if (this->granted_votes_ > (this->nodes_.size() / 2)) {
          TraceLog("DEBUG: ",
                   " node ",
                   this->id_,
                   " get majority votes in term ",
                   this->current_term_);
          this->BecomeLeader();
          this->SendHeartBeat();
          this->SendAppendEntries();
          this->granted_votes_ = 0;
        }
      } else {
        if (resp->leader_id() != -1) {
          this->leader_id_ = resp->leader_id();
          this->current_term_ = resp->term();
          this->BecomeFollower();
          this->voted_for_ = -1;
          this->store_->SaveRaftMeta(
              this, this->current_term_, this->voted_for_);
        }
        if (resp->term() >= this->current_term_) {
          this->BecomeFollower();
          this->voted_for_ = -1;
          this->store_->SaveRaftMeta(
              this, this->current_term_, this->voted_for_);
        }
      }
    }
  }
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param from_node
 * @param req
 * @param resp
 * @return EStatus
 */
EStatus RaftServer::HandleAppendEntriesReq(RaftNode*                  from_node,
                                           eraftkv::AppendEntriesReq* req,
                                           eraftkv::AppendEntriesResp* resp) 
{
  ResetRandomElectionTimeout();
  election_tick_count_ = 0;

  if(req->is_heartbeat()) {
    TraceLog("DEBUG: recv heart beat");
    this->AdvanceCommitIndexForFollower(req->leader_commit());
    resp->set_success(true);
    this->leader_id_ = req->leader_id();
    this->current_term_ = req->term();
    this->BecomeFollower();
    return EStatus::kOk;
  }


  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param from_node
 * @param resp
 * @return EStatus
 */
EStatus RaftServer::HandleAppendEntriesResp(RaftNode* from_node,
                                            eraftkv::AppendEntriesReq*  req,
                                            eraftkv::AppendEntriesResp* resp) {
  
  return EStatus::kOk;
}


/**
 * @brief
 *
 * @param from_node
 * @param req
 * @param resp
 * @return EStatus
 */
EStatus RaftServer::HandleSnapshotReq(RaftNode*              from_node,
                                      eraftkv::SnapshotReq*  req,
                                      eraftkv::SnapshotResp* resp) {
  return EStatus::kOk;
}

/**
 * @brief the log entry with index match term
 * 
 * @param term 
 * @param index 
 * @return true 
 * @return false 
 */
bool RaftServer::MatchLog(int64_t term, int64_t index) {
  return (index <= this->log_store_->LastIndex() &&
      index >= this->log_store_->FirstIndex()
      && this->log_store_->Get(index)->term() == term);
}


EStatus RaftServer::AdvanceCommitIndexForLeader() 
{
  std::vector<int64_t> match_idxs;
  for (auto node: this->nodes_) {
    match_idxs.push_back(node->match_log_index);
  }
  sort(match_idxs.begin(), match_idxs.end());
  int64_t new_commit_index = match_idxs[match_idxs.size() / 2];
  if (new_commit_index > this->commit_idx_) {
    if (this->MatchLog(this->current_term_, new_commit_index)) {
      this->commit_idx_ = new_commit_index;
      this->log_store_->PersisLogMetaState(this->commit_idx_, this->last_applied_idx_);
    }
  }
  return EStatus::kOk;
}

EStatus RaftServer::AdvanceCommitIndexForFollower(int64_t leader_commit)
{

  int64_t new_commit_index = std::min(leader_commit, this->log_store_->GetLastEty()->id());
  if(new_commit_index > this->commit_idx_) {
    this->commit_idx_ = new_commit_index;
    this->log_store_->PersisLogMetaState(this->commit_idx_, this->last_applied_idx_);
  }
  return EStatus::kOk;
}



/**
 * @brief
 *
 * @param from_node
 * @param resp
 * @return EStatus
 */
EStatus RaftServer::HandleSnapshotResp(RaftNode*              from_node,
                                       eraftkv::SnapshotResp* resp) {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param from_node
 * @param ety
 * @param ety_index
 * @return EStatus
 */
EStatus RaftServer::HandleApplyConfigChange(RaftNode*       from_node,
                                            eraftkv::Entry* ety,
                                            int64_t         ety_index) {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param ety
 * @return EStatus
 */
EStatus RaftServer::ProposeEntry(eraftkv::Entry* ety) {
  return EStatus::kOk;
}


/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::BecomeLeader() {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::BecomeFollower() {
  // TODO: stop heartbeat
  this->role_ = NodeRaftRoleEnum::Follower;
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::BecomeCandidate() {
  if (this->role_ == NodeRaftRoleEnum::Candidate) {
    return EStatus::kOk;
  }
  this->role_ = NodeRaftRoleEnum::Candidate;
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::BecomePreCandidate() {

  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param is_prevote
 * @return EStatus
 */
EStatus RaftServer::ElectionStart(bool is_prevote) {
  TraceLog("DEBUG: ",
           this->id_,
           " start a new election in term ",
           this->current_term_);
  this->granted_votes_ = 1;
  this->leader_id_ = -1;
  this->voted_for_ = this->id_;

  eraftkv::RequestVoteReq* vote_req = new eraftkv::RequestVoteReq();
  vote_req->set_term(this->current_term_);
  vote_req->set_candidtate_id(this->id_);
  vote_req->set_last_log_idx(this->log_store_->LastIndex());
  vote_req->set_last_log_term(this->log_store_->GetLastEty()->term());

  for (auto node : this->nodes_) {
    if (node->id == this->id_ || this->role_ == NodeRaftRoleEnum::Leader) {
      continue;
    }

    TraceLog("DEBUG: ",
             "send request vote to ",
             node->address,
             vote_req->SerializeAsString());
    this->net_->SendRequestVote(this, node, vote_req);
  }

  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::BeginSnapshot() {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::EndSnapshot() {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool RaftServer::SnapshotRunning() {
  return false;
}

/**
 * @brief Get the Last Applied Entry object
 *
 * @return Entry*
 */
eraftkv::Entry* RaftServer::GetLastAppliedEntry() {
  return nullptr;
}

/**
 * @brief Get the First Entry Idx object
 *
 * @return int64_t
 */
int64_t RaftServer::GetFirstEntryIdx() {
  return 0;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::RestoreSnapshotAfterRestart() {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @param last_included_term
 * @param last_included_index
 * @return EStatus
 */
EStatus RaftServer::BeginLoadSnapshot(int64_t last_included_term,
                                      int64_t last_included_index) {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::EndLoadSnapshot() {
  return EStatus::kOk;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::ProposeReadReq() {
  return EStatus::kOk;
}

/**
 * @brief Get the Logs Count Can Snapshot object
 *
 * @return int64_t
 */
int64_t RaftServer::GetLogsCountCanSnapshot() {
  return 0;
}

/**
 * @brief
 *
 * @return EStatus
 */
EStatus RaftServer::RestoreLog() {
  return EStatus::kOk;
}