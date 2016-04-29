# pragma once

# include "rrdb.client.h"

namespace dsn { namespace apps { 
class rrdb_perf_test_client
    : public rrdb_client,
      public ::dsn::service::perf_client_helper
{
public:
    rrdb_perf_test_client(
        ::dsn::rpc_address server)
        : rrdb_client(server)
    {
    }

    void start_test()
    {
        perf_test_suite s;
        std::vector<perf_test_suite> suits;

        s.name = "rrdb.put";
        s.config_section = "task.RPC_RRDB_RRDB_PUT";
        s.send_one = [this](int payload_bytes, int key_space_size){this->send_one_put(payload_bytes, key_space_size); };
        s.cases.clear();
        load_suite_config(s);
        suits.push_back(s);

        s.name = "rrdb.remove";
        s.config_section = "task.RPC_RRDB_RRDB_REMOVE";
        s.send_one = [this](int payload_bytes, int key_space_size){this->send_one_remove(payload_bytes, key_space_size); };
        s.cases.clear();
        load_suite_config(s);
        suits.push_back(s);

        s.name = "rrdb.merge";
        s.config_section = "task.RPC_RRDB_RRDB_MERGE";
        s.send_one = [this](int payload_bytes, int key_space_size){this->send_one_merge(payload_bytes, key_space_size); };
        s.cases.clear();
        load_suite_config(s);
        suits.push_back(s);

        s.name = "rrdb.get";
        s.config_section = "task.RPC_RRDB_RRDB_GET";
        s.send_one = [this](int payload_bytes, int key_space_size){this->send_one_get(payload_bytes, key_space_size); };
        s.cases.clear();
        load_suite_config(s);
        suits.push_back(s);

        start(suits);
    }

    void send_one_put(int payload_bytes, int key_space_size)
    {
        update_request req;
        // TODO: randomize the value of req
        auto rs = random64(0, 10000000) % key_space_size;
        req.key = dsn::blob((const char*)&rs, 0, sizeof(rs));
        
        auto buffer = std::shared_ptr<char>((char*)dsn_transient_malloc(payload_bytes), [](char* ptr){
                dsn_transient_free(ptr);
        });
        req.value = dsn::blob(buffer, payload_bytes);

        put(
            req,
            [this, context = prepare_send_one()](error_code err, int&& resp)
            {
                end_send_one(context, err);
            },
            _timeout
            );
    }

    void send_one_remove(int payload_bytes, int key_space_size)
    {
        ::dsn::blob req;
        auto rs = random64(0, 10000000) % key_space_size;
        req = dsn::blob((const char*)&rs, 0, sizeof(rs));

        remove(
            req,
            [this, context = prepare_send_one()](error_code err, int&& resp)
            {
                end_send_one(context, err);
            },
            _timeout
            );
    }

    void send_one_merge(int payload_bytes, int key_space_size)
    {
        update_request req;
        auto rs = random64(0, 10000000) % key_space_size;
        req.key = dsn::blob((const char*)&rs, 0, sizeof(rs));

        auto buffer = std::shared_ptr<char>((char*)dsn_transient_malloc(payload_bytes), [](char* ptr) {
            dsn_transient_free(ptr);
        });
        req.value = dsn::blob(buffer, payload_bytes);

        merge(
            req,
            [this, context = prepare_send_one()](error_code err, int&& resp)
            {
                end_send_one(context, err);
            },
            _timeout
            );
    }

    void send_one_get(int payload_bytes, int key_space_size)
    {
        ::dsn::blob req;
        auto rs = random64(0, 10000000) % key_space_size;
        req = dsn::blob((const char*)&rs, 0, sizeof(rs));
        get(
            req,
            [this, context = prepare_send_one()](error_code err, read_response&& resp)
            {
                end_send_one(context, err);
            },
            _timeout
            );
    }
};

} } 