/*
 * MIT License
 *
 * Alexandria.org
 *
 * Copyright (c) 2021 Josef Cullhed, <info@alexandria.org>, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "config.h"
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstring>

template<typename DataRecord> class FullTextShard;

#include "FullTextIndex.h"
#include "FullTextResultSet.h"

#include "system/Logger.h"
#include "system/Profiler.h"

/*
File format explained

8 bytes = unsigned int number of keys = num_keys
8 bytes * num_keys = list of keys
8 bytes * num_keys = list of positions in file counted from data start
8 bytes * num_keys = list of lengths
[DATA]

*/

template<typename DataRecord>
class FullTextShard {

public:

	FullTextShard(const std::string &db_name, size_t shard_id);
	~FullTextShard();

	void find(uint64_t key, FullTextResultSet<DataRecord> *result_set) const;
	size_t read_key_pos(std::ifstream &reader, uint64_t key) const;
	size_t total_num_results(uint64_t key) const;

	std::string mountpoint() const;
	std::string filename() const;
	std::string key_filename() const;
	size_t shard_id() const;
	bool empty() const;

	size_t disk_size() const;

private:

	std::string m_db_name;
	size_t m_shard_id;
	
};

template<typename DataRecord>
FullTextShard<DataRecord>::FullTextShard(const std::string &db_name, size_t shard)
: m_db_name(db_name), m_shard_id(shard) {
}

template<typename DataRecord>
FullTextShard<DataRecord>::~FullTextShard() {
}

template<typename DataRecord>
void FullTextShard<DataRecord>::find(uint64_t key, FullTextResultSet<DataRecord> *result_set) const {

	std::ifstream reader(filename(), std::ios::binary);

	size_t key_pos = read_key_pos(reader, key);

	if (key_pos == SIZE_MAX) {
		result_set->resize(0);
		return;
	}

	// Read page.
	Profiler::instance prof2("read keys");
	reader.seekg(key_pos);

	size_t num_keys;
	reader.read((char *)&num_keys, sizeof(size_t));


	LOG_INFO("num keys: " + std::to_string(num_keys));

	uint64_t *keys = new uint64_t[num_keys];

	reader.read((char *)keys, num_keys * sizeof(uint64_t));

	prof2.stop();

	Profiler::instance prof3("find key");

	size_t key_data_pos = SIZE_MAX;
	for (size_t i = 0; i < num_keys; i++) {
		if (keys[i] == key) {
			key_data_pos = i;
		}
	}

	if (key_data_pos == SIZE_MAX) {
		result_set->resize(0);
		return;
	}

	prof3.stop();

	char buffer[64];

	Profiler::instance prof4("read data");

	// Read position and length.
	reader.seekg(key_pos + 8 + num_keys * 8 + key_data_pos * 8, std::ios::beg);
	reader.read(buffer, 8);
	size_t pos = *((size_t *)(&buffer[0]));

	reader.seekg(key_pos + 8 + (num_keys * 8)*2 + key_data_pos * 8, std::ios::beg);
	reader.read(buffer, 8);
	size_t len = *((size_t *)(&buffer[0]));

	reader.seekg(key_pos + 8 + (num_keys * 8)*3 + key_data_pos * 8, std::ios::beg);
	reader.read(buffer, 8);
	size_t total_num_results = *((size_t *)(&buffer[0]));

	reader.seekg(key_pos + 8 + (num_keys * 8)*4 + pos, std::ios::beg);

	size_t num_records = len / sizeof(DataRecord);
	if (num_records > Config::ft_max_results_per_section) num_records = Config::ft_max_results_per_section;

	result_set->prepare_sections(filename(), (size_t)reader.tellg(), len);
	result_set->read_to_section(0);
	result_set->resize(num_records);
	result_set->set_total_num_results(total_num_results);
}

/*
 * Reads the exact position of the key, returns SIZE_MAX if the key was not found.
 * */
template<typename DataRecord>
size_t FullTextShard<DataRecord>::read_key_pos(std::ifstream &reader, uint64_t key) const {

	Profiler::instance prof1("read_key_pos " + key_filename());

	const size_t hash_pos = key % Config::shard_hash_table_size;

	std::ifstream key_reader(key_filename(), std::ios::binary);

	key_reader.seekg(hash_pos * sizeof(size_t));

	size_t pos;
	key_reader.read((char *)&pos, sizeof(size_t));

	return pos;
}

template<typename DataRecord>
size_t FullTextShard<DataRecord>::total_num_results(uint64_t key) const {

	std::ifstream reader(filename(), std::ios::binary);

	size_t key_pos = read_key_pos(reader, key);

	if (key_pos == SIZE_MAX) {
		return 0;
	}

	// Read page.
	reader.seekg(key_pos);

	size_t num_keys;
	reader.read((char *)&num_keys, sizeof(size_t));

	uint64_t *keys = new uint64_t[num_keys];

	reader.read((char *)keys, num_keys * sizeof(uint64_t));

	size_t key_data_pos = SIZE_MAX;
	for (size_t i = 0; i < num_keys; i++) {
		if (keys[i] == key) {
			key_data_pos = i;
		}
	}

	if (key_data_pos == SIZE_MAX) {
		return 0;
	}

	char buffer[64];

	reader.seekg(key_pos + 8 + (num_keys * 8)*3 + key_data_pos * 8, std::ios::beg);
	reader.read(buffer, 8);
	size_t total_num_results = *((size_t *)(&buffer[0]));

	return total_num_results;
}

template<typename DataRecord>
std::string FullTextShard<DataRecord>::mountpoint() const {
	return std::to_string(m_shard_id % 8);
}

template<typename DataRecord>
std::string FullTextShard<DataRecord>::filename() const {
	return "/mnt/" + mountpoint() + "/full_text/fti_" + m_db_name + "_" + std::to_string(m_shard_id) + ".idx";
}

template<typename DataRecord>
std::string FullTextShard<DataRecord>::key_filename() const {
	return "/mnt/" + mountpoint() + "/full_text/fti_" + m_db_name + "_" + std::to_string(m_shard_id) + ".keys";
}

template<typename DataRecord>
size_t FullTextShard<DataRecord>::shard_id() const {
	return m_shard_id;
}

template<typename DataRecord>
size_t FullTextShard<DataRecord>::disk_size() const {
	std::ifstream reader(filename(), std::ios::binary);
	if (!reader.is_open()) return 0;
	reader.seekg(0, std::ios::end);
	const size_t file_size = reader.tellg();
	return file_size;
}

template<typename DataRecord>
bool FullTextShard<DataRecord>::empty() const {
	return disk_size() == 0;
}

