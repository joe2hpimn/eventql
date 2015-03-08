/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#define BOOST_NO_CXX11_NUMERIC_LIMITS 1
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "fnord-base/io/filerepository.h"
#include "fnord-base/io/fileutil.h"
#include "fnord-base/application.h"
#include "fnord-base/logging.h"
#include "fnord-base/random.h"
#include "fnord-base/thread/eventloop.h"
#include "fnord-base/thread/threadpool.h"
#include "fnord-base/wallclock.h"
#include "fnord-rpc/ServerGroup.h"
#include "fnord-rpc/RPC.h"
#include "fnord-rpc/RPCClient.h"
#include "fnord-base/cli/flagparser.h"
#include "fnord-json/json.h"
#include "fnord-json/jsonrpc.h"
#include "fnord-http/httprouter.h"
#include "fnord-http/httpserver.h"
#include "fnord-feeds/FeedService.h"
#include "fnord-feeds/RemoteFeedFactory.h"
#include "fnord-feeds/RemoteFeedReader.h"
#include "fnord-base/stats/statsdagent.h"
#include "fnord-fts/fts.h"
#include "fnord-fts/fts_common.h"
#include "fnord-mdb/MDB.h"
#include "CustomerNamespace.h"
#include "FeatureSchema.h"
#include "IndexRequest.h"
#include "index/IndexBuilder.h"

using namespace cm;
using namespace fnord;

std::atomic<bool> cm_indexbuild_shutdown;

void quit(int n) {
  cm_indexbuild_shutdown = true;
}

int main(int argc, const char** argv) {
  Application::init();
  Application::logToStderr();

  /* shutdown hook */
  cm_indexbuild_shutdown = false;
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = quit;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  cli::FlagParser flags;

  flags.defineFlag(
      "cmdata",
      cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "clickmatcher app data dir",
      "<path>");

  flags.defineFlag(
      "cmcustomer",
      cli::FlagParser::T_STRING,
      true,
      NULL,
      NULL,
      "clickmatcher customer",
      "<key>");

  flags.defineFlag(
      "statsd_addr",
      fnord::cli::FlagParser::T_STRING,
      false,
      NULL,
      "127.0.0.1:8192",
      "Statsd addr",
      "<addr>");

  flags.defineFlag(
      "batch_size",
      fnord::cli::FlagParser::T_INTEGER,
      false,
      NULL,
      "2048",
      "batch_size",
      "<num>");

  flags.defineFlag(
      "buffer_size",
      fnord::cli::FlagParser::T_INTEGER,
      false,
      NULL,
      "8192",
      "buffer_size",
      "<num>");

  flags.defineFlag(
      "db_commit_size",
      fnord::cli::FlagParser::T_INTEGER,
      false,
      NULL,
      "1024",
      "db_commit_size",
      "<num>");

  flags.defineFlag(
      "db_commit_interval",
      fnord::cli::FlagParser::T_INTEGER,
      false,
      NULL,
      "5",
      "db_commit_interval",
      "<num>");

  flags.defineFlag(
      "dbsize",
      fnord::cli::FlagParser::T_INTEGER,
      false,
      NULL,
      "128",
      "max sessiondb size",
      "<MB>");

  flags.defineFlag(
      "loglevel",
      fnord::cli::FlagParser::T_STRING,
      false,
      NULL,
      "INFO",
      "loglevel",
      "<level>");

  flags.parseArgv(argc, argv);

  Logger::get()->setMinimumLogLevel(
      strToLogLevel(flags.getString("loglevel")));

  /* set up cmdata */
  auto cmdata_path = flags.getString("cmdata");
  if (!FileUtil::isDirectory(cmdata_path)) {
    RAISEF(kIOError, "no such directory: $0", cmdata_path);
  }

  /* set up feature schema */
  FeatureSchema feature_schema;
  feature_schema.registerFeature("shop_id", 1, 1);
  feature_schema.registerFeature("category1", 2, 1);
  feature_schema.registerFeature("category2", 3, 1);
  feature_schema.registerFeature("category3", 4, 1);
  feature_schema.registerFeature("title~de", 5, 2);

  /* start event loop */
  fnord::thread::EventLoop ev;

  auto evloop_thread = std::thread([&ev] {
    ev.run();
  });

  /* set up rpc client */
  HTTPRPCClient rpc_client(&ev);

  /* start stats reporting */
  fnord::stats::StatsdAgent statsd_agent(
      fnord::net::InetAddr::resolve(flags.getString("statsd_addr")),
      10 * fnord::kMicrosPerSecond);

  statsd_agent.start();

  /* set up indexbuild */
  auto cmcustomer = flags.getString("cmcustomer");
  size_t batch_size = flags.getInt("batch_size");
  size_t buffer_size = flags.getInt("buffer_size");
  size_t db_commit_size = flags.getInt("db_commit_size");
  size_t db_commit_interval = flags.getInt("db_commit_interval");

  fnord::logInfo(
      "cm.indexbuild",
      "Starting cm-indexbuild with:\n    customer=$0\n    batch_size=$1\n" \
      "    buffer_size=$2\n    db_commit_size=$3\n    db_commit_interval=$4\n"
      "    max_dbsize=$5MB",
      cmcustomer,
      batch_size,
      buffer_size,
      db_commit_size,
      db_commit_interval,
      flags.getInt("dbsize"));

  cm::IndexBuilder index_builder(&feature_schema);

  /* stats */
  fnord::stats::Counter<uint64_t> stat_documents_indexed_total_;
  fnord::stats::Counter<uint64_t> stat_documents_indexed_success_;
  fnord::stats::Counter<uint64_t> stat_documents_indexed_error_;

  exportStat(
      "/cm-indexbuild/global/documents_indexed_total",
      &stat_documents_indexed_total_,
      fnord::stats::ExportMode::EXPORT_DELTA);

  exportStat(
      "/cm-indexbuild/global/documents_indexed_success",
      &stat_documents_indexed_success_,
      fnord::stats::ExportMode::EXPORT_DELTA);

  exportStat(
      "/cm-indexbuild/global/documents_indexed_error",
      &stat_documents_indexed_error_,
      fnord::stats::ExportMode::EXPORT_DELTA);

  /* open featuredb db */
  auto featuredb_path = FileUtil::joinPaths(
      cmdata_path,
      StringUtil::format("index/$0/db", cmcustomer));

  FileUtil::mkdir_p(featuredb_path);
  auto featuredb = mdb::MDB::open(featuredb_path);
  featuredb->setMaxSize(1000000 * flags.getInt("dbsize"));

  /* open lucene index */
  /*
  auto index_path = FileUtil::joinPaths(
      cmdata_path,
      StringUtil::format("index/$0/fts", cmcustomer));
  FileUtil::mkdir_p(index_path);

  auto index_writer =
      fts::newLucene<fts::IndexWriter>(
          fts::FSDirectory::open(StringUtil::convertUTF8To16(index_path)),
          fts::newLucene<fts::StandardAnalyzer>(
              fts::LuceneVersion::LUCENE_CURRENT),
          true,
          fts::IndexWriter::MaxFieldLengthLIMITED);
  */

  /* set up input feed reader */
  feeds::RemoteFeedReader feed_reader(&rpc_client);
  feed_reader.setMaxSpread(3600 * 24 * 30 * kMicrosPerSecond);

  HashMap<String, URI> input_feeds;
  input_feeds.emplace(
      StringUtil::format(
          "$0.index_requests.feedserver01.nue01.production.fnrd.net",
          cmcustomer),
      URI("http://s01.nue01.production.fnrd.net:7001/rpc"));
  input_feeds.emplace(
      StringUtil::format(
          "$0.index_requests.feedserver02.nue01.production.fnrd.net",
          cmcustomer),
      URI("http://s02.nue01.production.fnrd.net:7001/rpc"));

  /* resume from last offset */
  auto txn = featuredb->startTransaction(true);
  try {
    for (const auto& input_feed : input_feeds) {
      uint64_t offset = 0;

      auto last_offset = txn->get(
          StringUtil::format("__indexfeed_offset~$0", input_feed.first));

      if (!last_offset.isEmpty()) {
        offset = std::stoul(last_offset.get().toString());
      }

      fnord::logInfo(
          "cm.indexbuild",
          "Adding source feed:\n    feed=$0\n    url=$1\n    offset: $2",
          input_feed.first,
          input_feed.second.toString(),
          offset);

      feed_reader.addSourceFeed(
          input_feed.second,
          input_feed.first,
          offset,
          batch_size,
          buffer_size);
    }

    txn->abort();
  } catch (...) {
    txn->abort();
    throw;
  }

  fnord::logInfo("cm.indexbuild", "Resuming IndexBuild...");
  // index document
/*
  auto doc = fts::newLucene<fts::Document>();
  doc->add(
      fts::newLucene<fts::Field>(
          L"keywords",
          L"my fnordy document",
          fts::Field::STORE_NO,
          fts::Field::INDEX_ANALYZED));

  index_writer->addDocument(doc);
*/


  //index_writer->commit();
  //index_writer->close();

  // simple search
  /*
  auto index_reader = fts::IndexReader::open(
      fts::FSDirectory::open(StringUtil::convertUTF8To16(index_path)),
      true);

  auto searcher = fts::newLucene<fts::IndexSearcher>(index_reader);

  auto analyzer = fts::newLucene<fts::StandardAnalyzer>(
      fts::LuceneVersion::LUCENE_CURRENT);

  auto query_parser = fts::newLucene<fts::QueryParser>(
      fts::LuceneVersion::LUCENE_CURRENT,
      L"keywords",
      analyzer);

  auto collector = fts::TopScoreDocCollector::create(
      500,
      false);

  auto query = query_parser->parse(L"fnordy");

  searcher->search(query, collector);
  fnord::iputs("found $0 documents", collector->getTotalHits());
  */

  DateTime last_iter;
  uint64_t rate_limit_micros = db_commit_interval * kMicrosPerSecond;

  for (;;) {
    last_iter = WallClock::now();
    feed_reader.fillBuffers();
    auto txn = featuredb->startTransaction();

    int i = 0;
    for (; i < db_commit_size; ++i) {
      auto entry = feed_reader.fetchNextEntry();

      if (entry.isEmpty()) {
        break;
      }

      try {
        stat_documents_indexed_total_.incr(1);
        fnord::logTrace("cm.indexbuild", "Indexing: $0", entry.get().data);
        auto index_req = json::fromJSON<cm::IndexRequest>(entry.get().data);
        index_builder.indexDocument(index_req, txn.get());
        stat_documents_indexed_success_.incr(1);
      } catch (const std::exception& e) {
        stat_documents_indexed_error_.incr(1);
        fnord::logError(
            "cm.indexbuild",
            e,
            "error while indexing document: $0",
            entry.get().data);
      }
    }

    auto stream_offsets = feed_reader.streamOffsets();
    String stream_offsets_str;

    for (const auto& soff : stream_offsets) {
      txn->update(
          StringUtil::format("__indexfeed_offset~$0", soff.first),
          StringUtil::toString(soff.second));

      stream_offsets_str +=
          StringUtil::format("\n    offset[$0]=$1", soff.first, soff.second);
    }

    fnord::logInfo(
        "cm.indexbuild",
        "IndexBuild comitting...$0",
        stream_offsets_str);

    txn->commit();

    if (cm_indexbuild_shutdown.load() == true) {
      break;
    }

    auto etime = WallClock::now().unixMicros() - last_iter.unixMicros();
    if (i < 1 && etime < rate_limit_micros) {
      usleep(rate_limit_micros - etime);
    }
  }

  ev.shutdown();
  //evloop_thread.join();

  fnord::logInfo("cm.indexbuild", "IndexBuild exiting...");
  exit(0); // FIXPAUL

  return 0;
}

