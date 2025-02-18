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

#include <iostream>
#include <vector>

class FullTextRecord;
template<typename DataRecord> class FullTextIndex;
class HashTable;
namespace Link {
	struct FullTextRecord;
}
namespace DomainLink {
	struct FullTextRecord;
}
namespace SearchAllocation {
	struct Allocation;
}

namespace Api {

	void search(const std::string &query, HashTable &hash_table, const FullTextIndex<FullTextRecord> &index,
		SearchAllocation::Allocation *allocation, std::stringstream &response_stream);

	void search(const std::string &query, HashTable &hash_table, const FullTextIndex<FullTextRecord> &index,
		const FullTextIndex<Link::FullTextRecord> &link_index,
		SearchAllocation::Allocation *allocation, std::stringstream &response_stream);

	void search(const std::string &query, HashTable &hash_table, const FullTextIndex<FullTextRecord> &index,
		const FullTextIndex<Link::FullTextRecord> &link_index, const FullTextIndex<DomainLink::FullTextRecord> &domain_link_index,
		SearchAllocation::Allocation *allocation, std::stringstream &response_stream);

	void search_all(const std::string &query, HashTable &hash_table, const FullTextIndex<FullTextRecord> &index,
		SearchAllocation::Allocation *allocation, std::stringstream &response_stream);

	void search_all(const std::string &query, HashTable &hash_table, const FullTextIndex<FullTextRecord> &index,
		const FullTextIndex<Link::FullTextRecord> &link_index,
		SearchAllocation::Allocation *allocation, std::stringstream &response_stream);

	void search_all(const std::string &query, HashTable &hash_table, const FullTextIndex<FullTextRecord> &index,
		const FullTextIndex<Link::FullTextRecord> &link_index, const FullTextIndex<DomainLink::FullTextRecord> &domain_link_index,
		SearchAllocation::Allocation *allocation, std::stringstream &response_stream);

	void word_stats(const std::string &query, const FullTextIndex<FullTextRecord> &index, const FullTextIndex<Link::FullTextRecord> &link_index,
		size_t index_size, size_t link_index_size, std::stringstream &response_stream);

	void url(const std::string &url_str, HashTable &hash_table, std::stringstream &response_stream);

	void ids(const std::string &query, const FullTextIndex<FullTextRecord> &index, SearchAllocation::Allocation *allocation,
		std::stringstream &response_stream);

	/*
	 * Make search on remote server but with links and url index on this server.
	 * */
	void search_remote(const std::string &query, HashTable &hash_table, const FullTextIndex<Link::FullTextRecord> &link_index,
		const FullTextIndex<DomainLink::FullTextRecord> &domain_link_index, SearchAllocation::Allocation *allocation,
		std::stringstream &response_stream);

}
