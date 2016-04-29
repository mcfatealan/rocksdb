# pragma once

# include "rrdb.server.h"
# include <rocksdb/db.h>
# include <vector>
# include <dsn/cpp/replicated_service_app.h>

namespace dsn {
    namespace apps {
        class rrdb_service_impl : 
            public rrdb_service,
            public replicated_service_app_type_1
        {
        public:
            rrdb_service_impl();

            // the following methods may set physical error if internal error occurs
            virtual void on_put(const update_request& update, ::dsn::rpc_replier<int>& reply) override;
            virtual void on_remove(const ::dsn::blob& key, ::dsn::rpc_replier<int>& reply) override;
            virtual void on_merge(const update_request& update, ::dsn::rpc_replier<int>& reply) override;
            virtual void on_get(const ::dsn::blob& key, ::dsn::rpc_replier<read_response>& reply) override;

            // open the db
            // if create_new == true, then first clear data and then create new db
            // returns:
            //  - ERR_OK
            //  - ERR_FILE_OPERATION_FAILED
            //  - ERR_LOCAL_APP_FAILURE
            virtual ::dsn::error_code start(int argc, char** argv) override;

            // close the db
            // if clear_state == true, then clear data after close the db
            // returns:
            //  - ERR_OK
            //  - ERR_FILE_OPERATION_FAILED
            virtual ::dsn::error_code stop(bool clear_state) override;

            // sync generate checkpoint
            // returns:
            //  - ERR_OK
            //  - ERR_WRONG_TIMING
            //  - ERR_NO_NEED_OPERATE
            //  - ERR_LOCAL_APP_FAILURE
            //  - ERR_FILE_OPERATION_FAILED
            virtual ::dsn::error_code checkpoint() override;

            // async generate checkpoint
            // returns:
            //  - ERR_OK
            //  - ERR_WRONG_TIMING
            //  - ERR_NO_NEED_OPERATE
            //  - ERR_LOCAL_APP_FAILURE
            //  - ERR_FILE_OPERATION_FAILED
            //  - ERR_TRY_AGAIN
            virtual ::dsn::error_code checkpoint_async() override;

            // get the last checkpoint
            // if succeed:
            //  - the checkpoint files path are put into "state.files"
            //  - the checkpoint_info are serialized into "state.meta"
            //  - the "state.from_decree_excluded" and "state.to_decree_excluded" are set properly
            // returns:
            //  - ERR_OK
            //  - ERR_OBJECT_NOT_FOUND
            //  - ERR_FILE_OPERATION_FAILED
            virtual ::dsn::error_code get_checkpoint(
                int64_t start,
                void*   learn_request,
                int     learn_request_size,
                /* inout */ app_learn_state& state
                ) override;

            // apply checkpoint, this will clear and recreate the db
            // if succeed:
            //  - last_committed_decree() == last_durable_decree()
            // returns:
            //  - ERR_OK
            //  - ERR_FILE_OPERATION_FAILED
            //  - error code of close()
            //  - error code of open()
            //  - error code of checkpoint()
            virtual ::dsn::error_code apply_checkpoint(const dsn_app_learn_state& state, dsn_chkpt_apply_mode mode) override;

        private:
            struct checkpoint_info
            {
                int64_t d;
                rocksdb::SequenceNumber    seq;
                checkpoint_info() : d(0), seq(0) {}
                // sort by first decree then seq increasingly
                bool operator< (const checkpoint_info& o) const
                {
                    return d < o.d || (d == o.d && seq < o.seq);
                }
            };

            // parse checkpoint directories in the data dir
            // checkpoint directory format is: "checkpoint.{decree}.{seq}"
            // returns the last checkpoint info
            checkpoint_info parse_checkpoints();

            // garbage collection checkpoints according to _max_checkpoint_count
            void gc_checkpoints();

            // write batch data into rocksdb
            void write_batch();

            // check _last_seq consistency
            void check_last_seq();

            // when in catch mode, increase _last_seq by one
            void catchup_one();

            const char* data_dir() { return _app_info->data_dir; }
            int64_t last_committed_decree() { return _app_info->info.type1.last_committed_decree; }
            int64_t last_durable_decree() { return _app_info->info.type1.last_durable_decree; }
            void    set_last_durable_decree(int64_t d) { _app_info->info.type1.last_durable_decree = d; }
            
            // init the commit decree, usually used by apps when initializing the 
            // state from checkpoints (e.g., update durable and commit decrees)
            void init_last_commit_decree(int64_t d) { _app_info->info.type1.last_committed_decree = d; }

            // see comments for batch_state, this function is not thread safe
            dsn_batch_state get_current_batch_state() { return _app_info->info.type1.batch_state; }

            void set_physical_error(int status) { _app_info->info.type1.physical_error = status;  }

        private:
            rocksdb::DB           *_db;
            rocksdb::WriteBatch   _batch;
            std::vector<rpc_replier<int>> _batch_repliers;

            rocksdb::Options      _db_opts;
            rocksdb::WriteOptions _wt_opts;
            rocksdb::ReadOptions  _rd_opts;

            volatile bool         _is_open;
            const int             _max_checkpoint_count;

            // ATTENTION:
            // _last_committed_decree is totally controlled by rdsn, and set to the decree of last checkpoint when open.
            // _last_durable_decree is always set to the decree of last checkpoint.
                        
            rocksdb::SequenceNumber      _last_seq;         // always equal to DB::GetLatestSequenceNumber()
            volatile bool                _is_catchup;       // whether the db is in catch up mode
            rocksdb::SequenceNumber      _last_durable_seq; // valid only when _is_catchup is true
            volatile bool                _is_checkpointing; // whether the db is doing checkpoint
            ::dsn::utils::ex_lock_nr     _checkpoints_lock; // protected the following checkpoints vector
            std::vector<checkpoint_info> _checkpoints;      // ordered checkpoints
            dsn_app_info*                _app_info;         // information from layer 2 (e.g., replication layer)
        };

        // --------- inline implementations -----------------
    }
}
