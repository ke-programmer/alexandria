
#include "FullTextIndex.h"
#include "system/Logger.h"

FullTextIndex::FullTextIndex(const string &db_name)
: m_db_name(db_name)
{
	for (size_t shard_id = 0; shard_id < FT_NUM_SHARDS; shard_id++) {
		m_shards.push_back(new FullTextShard(m_db_name, shard_id));
	}
	read_url_to_domain();
}

FullTextIndex::~FullTextIndex() {
	for (FullTextShard *shard : m_shards) {
		delete shard;
	}
}

vector<FullTextResult> FullTextIndex::search_word(const string &word) {
	uint64_t word_hash = m_hasher(word);

	FullTextResultSet *results = new FullTextResultSet();
	m_shards[word_hash % FT_NUM_SHARDS]->find(word_hash, results);
	LogInfo("Searched for " + word + " and found " + to_string(results->len()) + " results");

	vector<FullTextResult> ret;
	auto values = results->value_pointer();
	auto scores = results->score_pointer();
	for (size_t i = 0; i < results->len(); i++) {
		ret.push_back(FullTextResult(values[i], scores[i]));
	}
	delete results;

	sort_results(ret);

	return ret;
}

vector<FullTextResult> FullTextIndex::search_phrase(const string &phrase, int limit, size_t &total_found) {

	total_found = 0;

	vector<string> words = get_full_text_words(phrase);

	vector<string> searched_words;
	map<size_t, FullTextResultSet *> result_map;
	map<size_t, FullTextResultSet *> or_result_map;
	
	size_t idx = 0;
	double total_time = 0.0;
	for (const string &word : words) {

		// One word should only be searched once.
		if (find(searched_words.begin(), searched_words.end(), word) != searched_words.end()) continue;
		
		searched_words.push_back(word);

		uint64_t word_hash = m_hasher(word);
		Profiler profiler0("->find: " + word);
		FullTextResultSet *results = new FullTextResultSet();

		m_shards[word_hash % FT_NUM_SHARDS]->find(word_hash, results);
		profiler0.stop();

		Profiler profiler1("Accumuating "+to_string(results->len())+" results for: " + word);

		if (results->total_num_results() > results->len()) {
			or_result_map[idx] = results;
		} else {
			result_map[idx] = results;
		}
		idx++;
		total_time += profiler1.get();
	}

	if (result_map.size() == 0) {
		result_map = or_result_map;
		for (auto &iter : or_result_map) {
			if (iter.second->total_num_results() > total_found) {
				total_found = iter.second->total_num_results();
			}
		}
		or_result_map.clear();
	}

	Profiler profiler2("value_intersection");
	size_t shortest_vector;
	vector<size_t> result_ids = value_intersection(result_map, shortest_vector);

	vector<FullTextResult> result;

	FullTextResultSet *shortest = result_map[shortest_vector];
	vector<uint32_t> score_vector;
	uint64_t *value_arr = shortest->value_pointer();
	uint32_t *score_arr = shortest->score_pointer();
	for (size_t result_id : result_ids) {
		result.emplace_back(FullTextResult(value_arr[result_id], score_arr[result_id]));
		score_vector.push_back(score_arr[result_id]);
	}
	profiler2.stop();

	if (total_found == 0) {
		total_found = score_vector.size();
	}

	for (auto &iter : result_map) {
		delete iter.second;
	}
	for (auto &iter : or_result_map) {
		delete iter.second;
	}

	Profiler profiler3("sorting results");
	if (result.size() > limit) {
		nth_element(score_vector.begin(), score_vector.begin() + (limit - 1), score_vector.end());
		const uint32_t nth = score_vector[limit - 1];

		vector<FullTextResult> top_result;
		for (const FullTextResult &res : result) {
			if (res.m_score >= nth) {
				top_result.push_back(res);
				if (top_result.size() >= limit) break;
			}
		}

		sort_results(top_result);

		return top_result;
	}

	sort_results(result);

	return result;
}

size_t FullTextIndex::disk_size() const {
	size_t size = 0;
	for (auto shard : m_shards) {
		size += shard->disk_size();
	}
	return size;
}

void FullTextIndex::upload(const SubSystem *sub_system) {
	const size_t num_threads_uploading = 100;
	ThreadPool pool(num_threads_uploading);
	std::vector<std::future<void>> results;

	for (auto shard : m_shards) {
		results.emplace_back(
			pool.enqueue([this, sub_system, shard] {
				run_upload_thread(sub_system, shard);
			})
		);
	}

	for(auto && result: results) {
		result.get();
	}
}

void FullTextIndex::download(const SubSystem *sub_system) {
	const size_t num_threads_downloading = 100;
	ThreadPool pool(num_threads_downloading);
	std::vector<std::future<void>> results;

	for (auto shard : m_shards) {
		results.emplace_back(
			pool.enqueue([this, sub_system, shard] {
				run_download_thread(sub_system, shard);
			})
		);
	}

	for(auto && result: results) {
		result.get();
	}
}

vector<size_t> FullTextIndex::value_intersection(const map<size_t, FullTextResultSet *> &values_map,
	size_t &shortest_vector_position) const {
	Profiler value_intersection("FullTextIndex::value_intersection");
	
	if (values_map.size() == 0) return {};

	shortest_vector_position = 0;
	size_t shortest_len = SIZE_MAX;
	for (auto &iter : values_map) {
		if (shortest_len > iter.second->len()) {
			shortest_len = iter.second->len();
			shortest_vector_position = iter.first;
		}
	}

	vector<size_t> positions(values_map.size(), 0);
	vector<size_t> result_ids;

	auto value_iter = values_map.find(shortest_vector_position);
	uint64_t *value_ptr = value_iter->second->value_pointer();

	while (positions[shortest_vector_position] < shortest_len) {

		bool all_equal = true;
		uint64_t value = value_ptr[positions[shortest_vector_position]];

		for (auto &iter : values_map) {
			const uint64_t *val_arr = iter.second->value_pointer();
			const size_t len = iter.second->len();
			size_t *pos = &(positions[iter.first]);
			while (value > val_arr[*pos] && *pos < len) {
				(*pos)++;
			}
			if (value < val_arr[*pos] && *pos < len) {
				all_equal = false;
			}
			if (*pos >= len) {
				all_equal = false;
			}
		}
		if (all_equal) {
			result_ids.push_back(positions[shortest_vector_position]);
		}

		positions[shortest_vector_position]++;
	}

	return result_ids;
}

void FullTextIndex::sort_results(vector<FullTextResult> &results) {
	sort(results.begin(), results.end(), [](const FullTextResult a, const FullTextResult b) {
		return a.m_score > b.m_score;
	});
}

void FullTextIndex::run_upload_thread(const SubSystem *sub_system, const FullTextShard *shard) {
	ifstream infile(shard->filename());
	if (infile.is_open()) {
		const string key = "full_text/" + m_db_name + "/" + to_string(shard->shard_id()) + ".gz";
		sub_system->upload_from_stream("alexandria-index", key, infile);
	}
}

void FullTextIndex::run_download_thread(const SubSystem *sub_system, const FullTextShard *shard) {
	ofstream outfile(shard->filename(), ios::binary | ios::trunc);
	if (outfile.is_open()) {
		const string key = "full_text/" + m_db_name + "/" + to_string(shard->shard_id()) + ".gz";
		sub_system->download_to_stream("alexandria-index", key, outfile);
	}
}

void FullTextIndex::read_url_to_domain() {
	for (size_t bucket_id = 0; bucket_id < 8; bucket_id++) {
		const string file_name = "/mnt/"+(to_string(bucket_id))+"/full_text/url_to_domain_"+m_db_name+".fti";

		ifstream infile(file_name, ios::binary);
		if (infile.is_open()) {

			char buffer[8];

			do {

				infile.read(buffer, sizeof(uint64_t));
				if (infile.eof()) break;

				uint64_t url_hash = *((uint64_t *)buffer);

				infile.read(buffer, sizeof(uint64_t));
				uint64_t domain_hash = *((uint64_t *)buffer);

				m_url_to_domain[url_hash] = domain_hash;
				m_domains[domain_hash]++;

			} while (!infile.eof());

			infile.close();
		}
	}
}
