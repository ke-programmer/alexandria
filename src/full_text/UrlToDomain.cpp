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

#include "UrlToDomain.h"
#include "system/Logger.h"

using namespace std;

UrlToDomain::UrlToDomain(const string &db_name)
: m_db_name(db_name)
{
	
}

UrlToDomain::~UrlToDomain() {

}

void UrlToDomain::add_url(uint64_t url_hash, uint64_t domain_hash) {
	m_lock.lock();
	m_url_to_domain[url_hash] = domain_hash;
	m_domains[domain_hash]++;
	m_lock.unlock();
}

void UrlToDomain::read() {
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

void UrlToDomain::write(size_t indexer_id) {
	const string file_name = "/mnt/"+(to_string(indexer_id % 8))+"/full_text/url_to_domain_"+m_db_name+".fti";

	ofstream outfile(file_name, ios::binary | ios::app);
	if (!outfile.is_open()) {
		throw LOG_ERROR_EXCEPTION("Could not open url_to_domain file");
	}

	for (const auto &iter : m_url_to_domain) {
		outfile.write((const char *)&(iter.first), sizeof(uint64_t));
		outfile.write((const char *)&(iter.second), sizeof(uint64_t));
	}

	outfile.close();
}

void UrlToDomain::truncate() {
	for (size_t i = 0; i < 8; i++) {
		const string file_name = "/mnt/"+(to_string(i))+"/full_text/url_to_domain_"+m_db_name+".fti";
		ofstream outfile(file_name, ios::trunc);
	}
}

