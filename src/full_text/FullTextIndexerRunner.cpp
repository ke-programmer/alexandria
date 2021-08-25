
#include "config.h"
#include "FullTextIndexerRunner.h"
#include "FullText.h"
#include "FullTextIndexer.h"
#include <math.h>
#include "system/Logger.h"

FullTextIndexerRunner::FullTextIndexerRunner(const string &db_name, const string &hash_table_name, const string &cc_batch, const SubSystem *sub_system)
: m_cc_batch(cc_batch), m_db_name(db_name), m_hash_table_name(hash_table_name)
{
	m_sub_system = sub_system;
	m_did_allocate_sub_system = false;
}

FullTextIndexerRunner::FullTextIndexerRunner(const string &db_name, const string &hash_table_name, const string &cc_batch)
: m_cc_batch(cc_batch), m_db_name(db_name), m_hash_table_name(hash_table_name)
{
	m_sub_system = new SubSystem();
	m_did_allocate_sub_system = true;
}

FullTextIndexerRunner::FullTextIndexerRunner(const string &db_name, const string &hash_table_name, const SubSystem *sub_system)
: m_cc_batch("none"), m_db_name(db_name), m_hash_table_name(hash_table_name)
{
	m_sub_system = sub_system;
	m_did_allocate_sub_system = false;
}

FullTextIndexerRunner::~FullTextIndexerRunner() {
	if (m_did_allocate_sub_system) {
		delete m_sub_system;
	}
}

void FullTextIndexerRunner::run(size_t partition, size_t max_partitions) {

	truncate_cache();

	//TsvFileRemote manual_paths_file("crawl-data/ALEXANDRIA-MANUAL-01/warc.paths.gz");
	TsvFileRemote warc_paths_file(string("crawl-data/") + m_cc_batch + "/warc.paths.gz");

	vector<string> warc_paths_raw;
	warc_paths_file.read_column_into(0, warc_paths_raw, 50000);
	//manual_paths_file.read_column_into(0, warc_paths_raw);

	vector<string> warc_paths = FullText::make_partition_from_files(warc_paths_raw, partition, max_partitions);

	vector<vector<string>> warc_path_chunks;
	vector_chunk(warc_paths, ceil((float)warc_paths.size() / Config::ft_num_threads_indexing), warc_path_chunks);

	ThreadPool pool(Config::ft_num_threads_indexing);
	std::vector<std::future<string>> results;

	int id = 1;
	for (const vector<string> &warc_paths_chunk : warc_path_chunks) {

		results.emplace_back(
			pool.enqueue([this, warc_paths_chunk, id, partition] {
				return run_index_thread(warc_paths_chunk, id, partition);
			})
		);

		id++;

	}

	for(auto && result: results) {
		result.get();
	}

	merge();
	sort();

}

void FullTextIndexerRunner::run(const vector<string> local_files, size_t partition) {

	truncate_cache();

	ThreadPool pool(Config::ft_num_threads_indexing);
	std::vector<std::future<string>> results;

	vector<vector<string>> chunks;
	vector_chunk<string>(local_files, ceil(local_files.size() / Config::ft_num_threads_indexing), chunks);

	int id = 1;
	for (const vector<string> &chunk : chunks) {

		results.emplace_back(
			pool.enqueue([this, chunk, id, partition] {
				return run_index_thread_with_local_files(chunk, id, partition);
			})
		);

		id++;

	}

	for(auto && result: results) {
		result.get();
	}

	merge();
	sort();
}

void FullTextIndexerRunner::merge() {
	LogInfo("Merging...");
	Profiler profiler("Merging");

	const size_t merge_batch_size = 500;

	ThreadPool merge_pool(Config::ft_num_threads_merging);
	std::vector<std::future<string>> merge_results;

	// Loop over shards and merge them.
	for (size_t shard_id = 0; shard_id < Config::ft_num_shards; ) {

		while (shard_id < Config::ft_num_shards && merge_results.size() < merge_batch_size) {

			merge_results.emplace_back(
				merge_pool.enqueue([this, shard_id] {
					return run_merge_thread(shard_id);
				})
			);

			shard_id++;

		}

		for (auto && result: merge_results) {
			result.get();
		}
		merge_results.clear();
	}
}

void FullTextIndexerRunner::sort() {
	LogInfo("Sorting...");
	Profiler profiler("Sorting");

	// Loop over hash table shards and merge them.
	for (size_t shard_id = 0; shard_id < Config::ht_num_shards; shard_id++) {
		HashTableShardBuilder *shard = new HashTableShardBuilder(m_hash_table_name, shard_id);
		shard->sort();
		delete shard;
	}
}

void FullTextIndexerRunner::truncate_cache() {
	for (size_t shard_id = 0; shard_id < Config::ft_num_shards; shard_id++) {
		FullTextShardBuilder<struct FullTextRecord> *shard_builder =
			new FullTextShardBuilder<struct FullTextRecord>(m_db_name, shard_id);
		shard_builder->truncate_cache_files();
		delete shard_builder;
	}

}

void FullTextIndexerRunner::truncate() {
	for (size_t shard_id = 0; shard_id < Config::ft_num_shards; shard_id++) {
		FullTextShardBuilder<struct FullTextRecord> *shard_builder =
			new FullTextShardBuilder<struct FullTextRecord>(m_db_name, shard_id);
		shard_builder->truncate();
		delete shard_builder;
	}

}

string FullTextIndexerRunner::run_merge_large_thread() {

	UrlToDomain url_to_domain("main_index");
	FullTextIndexer indexer(1, m_db_name, m_sub_system, &url_to_domain);

	while (m_run_merge_large) {
		cout << "merged " << indexer.write_large(m_full_text_mutexes) << " large files" << endl;
		sleep(1);
	}

	return "done";
}

string FullTextIndexerRunner::run_index_thread(const vector<string> &warc_paths, int id, size_t partition) {

	vector<HashTableShardBuilder *> shard_builders;
	for (size_t i = 0; i < Config::ht_num_shards; i++) {
		shard_builders.push_back(new HashTableShardBuilder(m_hash_table_name, i));
	}

	UrlToDomain url_to_domain("main_index");
	FullTextIndexer indexer(id, m_db_name, m_sub_system, &url_to_domain);
	size_t idx = 1;
	for (const string &raw_warc_path : warc_paths) {
		stringstream stream;

		string warc_path = raw_warc_path;
		const size_t pos = warc_path.find(".warc.gz");
		if (pos != string::npos) {
			warc_path.replace(pos, 8, ".gz");
		}

		int error;
		Transfer::gz_file_to_stream(warc_path, stream, error);
		if (error == Transfer::OK) {
			indexer.add_stream(shard_builders, stream, {1, 2, 3, 4}, {10.0, 3.0, 2.0, 1}, partition);
			cout << "wrote " << indexer.write_cache(m_full_text_mutexes) << " out of " << Config::ft_num_shards << " shards" << endl;
		}

		for (size_t i = 0; i < Config::ht_num_shards; i++) {
			if (shard_builders[i]->full()) {
				m_hash_table_mutexes[i].lock();
				shard_builders[i]->write();
				m_hash_table_mutexes[i].unlock();
			}
		}

		LogInfo("Done " + to_string(idx) + " out of " + to_string(warc_paths.size()) + " for " + m_db_name);

		idx++;
	}
	indexer.flush_cache(m_full_text_mutexes);

	for (size_t i = 0; i < Config::ht_num_shards; i++) {
		m_hash_table_mutexes[i].lock();
		shard_builders[i]->write();
		m_hash_table_mutexes[i].unlock();
	}

	m_write_url_to_domain_mutex.lock();
	indexer.write_url_to_domain();
	m_write_url_to_domain_mutex.unlock();

	for (HashTableShardBuilder *shard_builder : shard_builders) {
		delete shard_builder;
	}

	return "";
}

string FullTextIndexerRunner::run_index_thread_with_local_files(const vector<string> &local_files, int id, size_t partition) {

	vector<HashTableShardBuilder *> shard_builders;
	for (size_t i = 0; i < Config::ht_num_shards; i++) {
		shard_builders.push_back(new HashTableShardBuilder(m_hash_table_name, i));
	}

	UrlToDomain url_to_domain("main_index");
	FullTextIndexer indexer(id, m_db_name, m_sub_system, &url_to_domain);
	size_t idx = 1;
	for (const string &local_file : local_files) {

		ifstream stream(local_file, ios::in);

		if (stream.is_open()) {
			size_t added_urls = indexer.add_stream(shard_builders, stream, {1, 2, 3, 4}, {10.0, 3.0, 2.0, 1}, partition);
			cout << "wrote " << indexer.write_cache(m_full_text_mutexes) << " out of " << Config::ft_num_shards << " shards and added " << added_urls << " to partition " << partition  << " on node id: " << Config::node_id << "/" << Config::nodes_in_cluster << endl;
		}

		stream.close();

		for (size_t i = 0; i < Config::ht_num_shards; i++) {
			if (shard_builders[i]->full()) {
				m_hash_table_mutexes[i].lock();
				shard_builders[i]->write();
				m_hash_table_mutexes[i].unlock();
			}
		}

		LogInfo("Done " + to_string(idx) + " out of " + to_string(local_files.size()) + " for " + m_db_name);

		idx++;
	}
	indexer.flush_cache(m_full_text_mutexes);

	for (size_t i = 0; i < Config::ht_num_shards; i++) {
		m_hash_table_mutexes[i].lock();
		shard_builders[i]->write();
		m_hash_table_mutexes[i].unlock();
	}

	m_write_url_to_domain_mutex.lock();
	indexer.write_url_to_domain();
	m_write_url_to_domain_mutex.unlock();

	for (HashTableShardBuilder *shard_builder : shard_builders) {
		delete shard_builder;
	}

	return "";
}

string FullTextIndexerRunner::run_merge_thread(size_t shard_id) {

	FullTextShardBuilder<struct FullTextRecord> shard(m_db_name, shard_id);
	shard.merge();

	return "";
}

int FullTextIndexerRunner::download_file(const string &bucket, const string &key, stringstream &stream) {

	Aws::S3::Model::GetObjectRequest request;
	cout << "Downloading " << bucket << " key: " << key << endl;
	request.SetBucket(bucket);
	request.SetKey(key);

	auto outcome = m_sub_system->s3_client().GetObject(request);

	if (outcome.IsSuccess()) {

		auto &input_stream = outcome.GetResultWithOwnership().GetBody();

		filtering_istream decompress_stream;
		decompress_stream.push(gzip_decompressor());
		decompress_stream.push(input_stream);

		stream << decompress_stream.rdbuf();

		return 0;
	}

	return 1;
}

