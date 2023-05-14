/**
 * @file rocksdb_storage_impl.h
 * @author ERaftGroup
 * @brief
 * @version 0.1
 * @date 2023-03-30
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ROCKSDB_STORAGE_IMPL_H_
#define ROCKSDB_STORAGE_IMPL_H_


#include <rocksdb/db.h>

#include "log_entry_cache.h"
#include "raft_server.h"

/**
 * @brief
 *
 */
class RocksDBStorageImpl : public Storage {

 public:
  /**
   * @brief Get the Node Address object
   *
   * @param raft
   * @param id
   * @return std::string
   */
  std::string GetNodeAddress(RaftServer* raft, std::string id);

  /**
   * @brief
   *
   * @param raft
   * @param id
   * @param address
   * @return EStatus
   */
  EStatus SaveNodeAddress(RaftServer* raft,
                          std::string id,
                          std::string address);

  /**
   * @brief
   *
   * @param raft
   * @param snapshot_index
   * @param snapshot_term
   * @return EStatus
   */
  EStatus ApplyLog(RaftServer* raft,
                   int64_t     snapshot_index,
                   int64_t     snapshot_term);

  /**
   * @brief Get the Snapshot Block object
   *
   * @param raft
   * @param node
   * @param offset
   * @param block
   * @return EStatus
   */
  EStatus GetSnapshotBlock(RaftServer*             raft,
                           RaftNode*               node,
                           int64_t                 offset,
                           eraftkv::SnapshotBlock* block);

  /**
   * @brief
   *
   * @param raft
   * @param snapshot_index
   * @param offset
   * @param block
   * @return EStatus
   */
  EStatus StoreSnapshotBlock(RaftServer*             raft,
                             int64_t                 snapshot_index,
                             int64_t                 offset,
                             eraftkv::SnapshotBlock* block);

  /**
   * @brief
   *
   * @param raft
   * @return EStatus
   */
  EStatus ClearSnapshot(RaftServer* raft);

  /**
   * @brief
   *
   * @return EStatus
   */
  EStatus CreateDBSnapshot();

  /**
   * @brief
   *
   * @param raft
   * @param term
   * @param vote
   * @return EStatus
   */
  EStatus SaveRaftMeta(RaftServer* raft, int64_t term, int64_t vote);

  /**
   * @brief
   *
   * @param raft
   * @param term
   * @param vote
   * @return EStatus
   */
  EStatus ReadRaftMeta(RaftServer* raft, int64_t* term, int64_t* vote);


  /**
   * @brief
   *
   * @param key
   * @param val
   * @return EStatus
   */
  EStatus PutKV(std::string key, std::string val);

  /**
   * @brief
   *
   * @param key
   * @return std::string
   */
  std::string GetKV(std::string key);

  /**
   * @brief Construct a new RocksDB Storage Impl object
   *
   * @param db_path
   */
  RocksDBStorageImpl(std::string db_path);

  /**
   * @brief Destroy the Rocks DB Storage Impl object
   *
   */
  ~RocksDBStorageImpl();

 private:
  /**
   * @brief
   *
   */
  std::string db_path_;

  /**
   * @brief
   *
   */
  rocksdb::DB* kv_db_;
};


struct LogDBStatus {
  int64_t     prev_log_term;
  int64_t     prev_log_index;
  int64_t     entries_count;
  int64_t     last_log_index;
  std::string db_path;
  int64_t     db_size;
};


/**
 * @brief
 *
 */
class RocksDBLogStorageImpl : public LogStore {

 public:
  /**
   * @brief
   *
   */
  RocksDBLogStorageImpl();

  /**
   * @brief
   *
   */
  ~RocksDBLogStorageImpl();


  EStatus Reset(int64_t index, int64_t term);

  EStatus Open(std::string logdb_path,
               int64_t     pre_log_term,
               int64_t     pre_log_index);

  /**
   * @brief
   *
   * @param ety
   * @return EStatus
   */
  EStatus Append(eraftkv::Entry* ety);

  /**
   * @brief
   *
   * @param first_index
   * @return EStatus
   */
  EStatus EraseBefore(int64_t first_index);

  /**
   * @brief
   *
   * @param from_index
   * @return EStatus
   */
  EStatus EraseAfter(int64_t from_index);

  /**
   * @brief
   *
   * @param index
   * @return eraftkv::Entry*
   */
  eraftkv::Entry* Get(int64_t index);

  /**
   * @brief
   *
   * @param start_index
   * @param end_index
   * @return std::vector<eraftkv::Entry*>
   */
  std::vector<eraftkv::Entry*> Gets(int64_t start_index, int64_t end_index);

  /**
   * @brief
   *
   * @return int64_t
   */
  int64_t FirstIndex();

  /**
   * @brief
   *
   * @return int64_t
   */
  int64_t LastIndex();

  /**
   * @brief
   *
   * @return int64_t
   */
  int64_t LogCount();

 private:
  /**
   * @brief
   *
   */
  uint64_t count_;
  /**
   * @brief
   *
   */
  std::string node_id_;

  /**
   * @brief
   *
   */
  rocksdb::DB* master_log_db_;

  /**
   * @brief
   *
   */
  LogDBStatus m_status_;

  /**
   * @brief
   *
   */
  rocksdb::DB* standby_log_db_;

  /**
   * @brief
   *
   */
  LogDBStatus s_status_;

  /**
   * @brief cache for log entry
   *
   */
  LogEntryCache* log_cache_;
};


class RocksDBSingleLogStorageImpl : public LogStore {

 public:
  RocksDBSingleLogStorageImpl();

  ~RocksDBSingleLogStorageImpl();

  /**
   * @brief Append add new entries
   *
   * @param ety
   * @return EStatus
   */
  EStatus Append(eraftkv::Entry* ety);

  /**
   * @brief EraseBefore erase all entries before the given index
   *
   * @param first_index
   * @return EStatus
   */
  EStatus EraseBefore(int64_t first_index);

  /**
   * @brief EraseAfter erase all entries after the given index
   *
   * @param from_index
   * @return EStatus
   */
  EStatus EraseAfter(int64_t from_index);

  /**
   * @brief Get get the given index entry
   *
   * @param index
   * @return eraftkv::Entry*
   */
  eraftkv::Entry* Get(int64_t index);

  /**
   * @brief Get the First Ety object
   *
   * @return eraftkv::Entry*
   */
  eraftkv::Entry* GetFirstEty();

  /**
   * @brief Get the Last Ety object
   *
   * @return eraftkv::Entry*
   */
  eraftkv::Entry* GetLastEty();

  /**
   * @brief
   *
   * @param start
   * @param end
   * @return EStatus
   */
  EStatus EraseRange(int64_t start, int64_t end);

  /**
   * @brief Gets get the given index range entry
   *
   * @param start_index
   * @param end_index
   * @return std::vector<eraftkv::Entry*>
   */
  std::vector<eraftkv::Entry*> Gets(int64_t start_index, int64_t end_index);

  /**
   * @brief FirstIndex get the first index in the entry
   *
   * @return int64_t
   */
  int64_t FirstIndex();

  /**
   * @brief LastIndex get the last index in the entry
   *
   * @return int64_t
   */
  int64_t LastIndex();

  /**
   * @brief LogCount get the number of entries
   *
   * @return int64_t
   */
  int64_t LogCount();

  EStatus PersisLogMetaState(int64_t commit_idx, int64_t applied_idx);

  EStatus ReadMetaState(int64_t* commit_idx, int64_t* applied_idx);

  int64_t first_idx;

  int64_t last_idx;

  int64_t snapshot_idx;

 private:
  int64_t commit_idx_;

  int64_t applied_idx_;


  rocksdb::DB* log_db_;
};

#endif
