/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "config.h"                         // for XAPIAND_DATABASE_WAL

#include <chrono>                           // for std::chrono, std::chrono::system_clock, std::chrono::time_point
#include <deque>                            // for std::deque
#include <memory>                           // for shared_ptr
#include <mutex>                            // for std::mutex, std::lock_guard
#include <set>                              // for std::set
#include <stdio.h>                          // for size_t
#include <string>                           // for std::string
#include <sys/types.h>                      // for ssize_t
#include <utility>                          // for std::pair
#include <vector>                           // for std::vector

#include "base_client.h"                    // for MetaBaseClient
#include "compressor_deflate.h"             // for DeflateCompressData
#include "database_data.h"                  // for ct_type_t, accept_set_t
#include "endpoint.h"                       // for Endpoints
#include "hashes.hh"                        // for hhl
#include "http_parser.h"                    // for http_parser, http_parser_settings
#include "lightweight_semaphore.h"          // for LightweightSemaphore
#include "lru.h"                            // for LRU
#include "msgpack.h"                        // for MsgPack
#include "phf.hh"                           // for phf::make_phf
#include "url_parser.h"                     // for PathParser, QueryParser


class DocIndexer;
class UUIDGenerator;
class Logging;
class Worker;
struct query_field_t;


#define HTTP_STATUS_RESPONSE            (1 << 0)
#define HTTP_HEADER_RESPONSE            (1 << 1)
#define HTTP_ACCEPT_RESPONSE            (1 << 2)
#define HTTP_BODY_RESPONSE              (1 << 3)
#define HTTP_CONTENT_TYPE_RESPONSE      (1 << 4)
#define HTTP_CONTENT_ENCODING_RESPONSE  (1 << 5)
#define HTTP_CONTENT_LENGTH_RESPONSE    (1 << 6)
#define HTTP_CHUNKED_RESPONSE           (1 << 7)
#define HTTP_OPTIONS_RESPONSE           (1 << 8)
#define HTTP_TOTAL_COUNT_RESPONSE       (1 << 9)
#define HTTP_MATCHES_ESTIMATED_RESPONSE (1 << 10)


class AcceptLRU : private lru::LRU<std::string, accept_set_t> {
	std::mutex qmtx;

public:
	AcceptLRU()
		: LRU<std::string, accept_set_t>(100) { }

	std::pair<bool, accept_set_t> lookup(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		auto it = LRU::find(key);
		if (it == LRU::end()) {
			return std::make_pair(false, accept_set_t{});
		}
		return std::make_pair(true, it->second);
	}

	auto emplace(std::string key, accept_set_t set) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::emplace(key, set);
	}
};


struct AcceptEncoding {
	int position;
	double priority;

	std::string encoding;

	AcceptEncoding(int position, double priority, std::string encoding) : position(position), priority(priority), encoding(encoding) { }
};
using accept_encoding_set_t = std::set<AcceptEncoding, accept_preference_comp<AcceptEncoding>>;


class AcceptEncodingLRU : private lru::LRU<std::string, accept_encoding_set_t> {
	std::mutex qmtx;

public:
	AcceptEncodingLRU()
	: LRU<std::string, accept_encoding_set_t>(100) { }

	std::pair<bool, accept_encoding_set_t> lookup(std::string key) {
		std::lock_guard<std::mutex> lk(qmtx);
		auto it = LRU::find(key);
		if (it == LRU::end()) {
			return std::make_pair(false, accept_encoding_set_t{});
		}
		return std::make_pair(true, it->second);
	}

	auto emplace(std::string key, accept_encoding_set_t set) {
		std::lock_guard<std::mutex> lk(qmtx);
		return LRU::emplace(key, set);
	}
};


enum class Encoding {
	none,
	gzip,
	deflate,
	identity,
	unknown,
};


// Available commands

constexpr const char COMMAND_CHECK[]       = COMMAND__ "check";
constexpr const char COMMAND_COMMIT[]      = COMMAND__ "commit";
constexpr const char COMMAND_COUNT[]       = COMMAND__ "count";
constexpr const char COMMAND_DUMP[]        = COMMAND__ "dump";
constexpr const char COMMAND_FLUSH[]       = COMMAND__ "flush";
constexpr const char COMMAND_INFO[]        = COMMAND__ "info";
constexpr const char COMMAND_METADATA[]    = COMMAND__ "metadata";
constexpr const char COMMAND_METRICS[]     = COMMAND__ "metrics";
constexpr const char COMMAND_NODES[]       = COMMAND__ "nodes";
constexpr const char COMMAND_QUIT[]        = COMMAND__ "quit";
constexpr const char COMMAND_RESTORE[]     = COMMAND__ "restore";
constexpr const char COMMAND_SCHEMA[]      = COMMAND__ "schema";
constexpr const char COMMAND_SEARCH[]      = COMMAND__ "search";
constexpr const char COMMAND_TOUCH[]       = COMMAND__ "touch";
constexpr const char COMMAND_WAL[]         = COMMAND__ "wal";

#define COMMAND_OPTIONS() \
	OPTION(CHECK) \
	OPTION(COMMIT) \
	OPTION(COUNT) \
	OPTION(DUMP) \
	OPTION(FLUSH) \
	OPTION(INFO) \
	OPTION(METADATA) \
	OPTION(METRICS) \
	OPTION(NODES) \
	OPTION(QUIT) \
	OPTION(RESTORE) \
	OPTION(SCHEMA) \
	OPTION(SEARCH) \
	OPTION(TOUCH) \
	OPTION(WAL)

constexpr static auto http_commands = phf::make_phf({
	#define OPTION(name) hhl(COMMAND_##name),
	COMMAND_OPTIONS()
	#undef OPTION
});


class Request;
class Response;
class HttpClient;
using view_function = void(HttpClient::*)(Request&);


class Response {
public:
	std::string head;
	std::string headers;
	std::string text;  // The text representation of the body (for logging purposes mostly) goes here

	ct_type_t ct_type;
	std::string blob;

	enum http_status status;
	size_t size;

	DeflateCompressData encoding_compressor;
	DeflateCompressData::iterator it_compressor;

	Response();

	Response(const Response&) = delete;
	Response(Response&&) = default;
	Response& operator=(const Response&) = delete;
	Response& operator=(Response&&) = default;

	std::string to_text(bool decode);
};


class Request {
	MsgPack _decoded_body;

	MsgPack decode(std::string_view body);

public:
	enum class Mode {
		FULL,
		STREAM,
		STREAM_NDJSON,
		STREAM_MSGPACK,
	} mode;

	Response response;

	view_function view;

	Encoding type_encoding;

	std::string _header_name;

	accept_set_t accept_set;
	accept_encoding_set_t accept_encoding_set;

	enum http_method method;
	std::string path;
	struct http_parser parser;

	std::string headers;
	std::string text;  // The text representation of the body (for logging purposes mostly) goes here

	bool begining;
	std::atomic_bool ending;  // ending requests have received all body
	bool ended;

	LightweightSemaphore pending;

	std::string raw;
	size_t raw_peek;
	size_t raw_offset;

	std::mutex objects_mtx;
	std::deque<MsgPack> objects;
	msgpack::unpacker unpacker;  // msgpack unpacker

	ct_type_t ct_type;

	size_t size;

	int indented;
	bool expect_100;
	bool closing;

	PathParser path_parser;
	QueryParser query_parser;

	std::shared_ptr<Logging> log;

	std::chrono::time_point<std::chrono::system_clock> begins;
	std::chrono::time_point<std::chrono::system_clock> received;
	std::chrono::time_point<std::chrono::system_clock> processing;
	std::chrono::time_point<std::chrono::system_clock> ready;
	std::chrono::time_point<std::chrono::system_clock> ends;

	std::shared_ptr<DocIndexer> indexer;

	Request() : view{nullptr} { }
	Request(class HttpClient* client);
	~Request() noexcept;

	Request(const Request&) = delete;
	Request(Request&&) = default;
	Request& operator=(const Request&) = delete;
	Request& operator=(Request&&) = default;

	bool append(const char* at, size_t length);

	bool wait();

	bool next(std::string_view& str_view);
	bool next_object(MsgPack& obj);

	MsgPack& decoded_body();

	std::string head();

	std::string to_text(bool decode);
};


// A single instance of a non-blocking Xapiand HTTP protocol handler.
class HttpClient : public MetaBaseClient<HttpClient> {
	friend MetaBaseClient<HttpClient>;

	enum class Command : uint32_t {
		#define OPTION(name) CMD_##name = http_commands.fhhl(COMMAND_##name),
		COMMAND_OPTIONS()
		#undef OPTION
		NO_CMD_NO_ID,
		NO_CMD_ID,
		BAD_QUERY,
	};

	template <typename Func>
	int handled_errors(Request& request, Func&& func);

	bool is_idle() const;

	Command getCommand(std::string_view command_name);

	ssize_t on_read(const char* buf, ssize_t received);
	void on_read_file(const char* buf, ssize_t received);
	void on_read_file_done();

	static const http_parser_settings settings;

	mutable std::mutex runner_mutex;
	std::shared_ptr<Request> new_request;
	std::deque<std::shared_ptr<Request>> requests;
	Endpoints endpoints;

	static int message_begin_cb(http_parser* parser);
	static int url_cb(http_parser* parser, const char* at, size_t length);
	static int status_cb(http_parser* parser, const char* at, size_t length);
	static int header_field_cb(http_parser* parser, const char* at, size_t length);
	static int header_value_cb(http_parser* parser, const char* at, size_t length);
	static int headers_complete_cb(http_parser* parser);
	static int body_cb(http_parser* parser, const char* at, size_t length);
	static int message_complete_cb(http_parser* parser);
	static int chunk_header_cb(http_parser* parser);
	static int chunk_complete_cb(http_parser* parser);

	int on_message_begin(http_parser* parser);
	int on_url(http_parser* parser, const char* at, size_t length);
	int on_status(http_parser* parser, const char* at, size_t length);
	int on_header_field(http_parser* parser, const char* at, size_t length);
	int on_header_value(http_parser* parser, const char* at, size_t length);
	int on_headers_complete(http_parser* parser);
	int on_body(http_parser* parser, const char* at, size_t length);
	int on_message_complete(http_parser* parser);
	int on_chunk_header(http_parser* parser);
	int on_chunk_complete(http_parser* parser);

	int prepare();
	view_function _prepare_options();
	view_function _prepare_head();
	view_function _prepare_get();
	view_function _prepare_merge();
	view_function _prepare_store();
	view_function _prepare_put();
	view_function _prepare_post();
	view_function _prepare_patch();
	view_function _prepare_delete();

	void home_view(Request& request);
	void metrics_view(Request& request);
	void info_view(Request& request);
	void metadata_view(Request& request);
	void write_metadata_view(Request& request);
	void update_metadata_view(Request& request);
	void delete_metadata_view(Request& request);
	void delete_document_view(Request& request);
	void delete_schema_view(Request& request);
	void index_document_view(Request& request);
	void write_schema_view(Request& request);
	void document_info_view(Request& request);
	void update_document_view(Request& request);
	void retrieve_view(Request& request);
	void search_view(Request& request);
	void count_view(Request& request);
	void touch_view(Request& request);
	void commit_view(Request& request);
	void dump_view(Request& request);
	void restore_view(Request& request);
	void schema_view(Request& request);
#if XAPIAND_DATABASE_WAL
	void wal_view(Request& request);
#endif
	void check_view(Request& request);
	void nodes_view(Request& request);

	Command url_resolve(Request& request);
	void _endpoint_maker(Request& request, bool master);
	void endpoints_maker(Request& request, bool master);
	query_field_t query_field_maker(Request& request, int flags);

	void log_request(Request& request);
	void log_response(Response& response);

	std::string http_response(Request& request, enum http_status status, int mode, int total_count = 0, int matches_estimated = 0, const std::string& body = "", const std::string& ct_type = "application/json; charset=UTF-8", const std::string& ct_encoding = "", size_t content_length = 0);
	void end_http_request(Request& request);
	std::pair<std::string, std::string> serialize_response(const MsgPack& obj, const ct_type_t& ct_type, int indent, bool serialize_error=false);

	ct_type_t resolve_ct_type(Request& request, ct_type_t ct_type_str);
	template <typename T>
	const ct_type_t& get_acceptable_type(Request& request, const T& ct);
	const ct_type_t* is_acceptable_type(const ct_type_t& ct_type_pattern, const ct_type_t& ct_type);
	const ct_type_t* is_acceptable_type(const ct_type_t& ct_type_pattern, const std::vector<ct_type_t>& ct_types);
	void write_status_response(Request& request, enum http_status status, const std::string& message="");
	void write_http_response(Request& request, enum http_status status, const MsgPack& obj=MsgPack());
	Encoding resolve_encoding(Request& request);
	std::string readable_encoding(Encoding e);
	std::string encoding_http_response(Response& response, Encoding e, const std::string& response_obj, bool chunk, bool start, bool end);

	friend Worker;

public:
	HttpClient(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int sock_);

	~HttpClient() noexcept;

	void process(Request& request);
	void operator()();

	std::string __repr__() const override;
};
