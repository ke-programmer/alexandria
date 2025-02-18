
cmake_minimum_required(VERSION 3.5)
set(CMAKE_C_COMPILER /usr/bin/gcc-10)
set(CMAKE_CXX_COMPILER /usr/bin/g++-10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(THREADS_PREFER_PTHREAD_FLAG ON)
project(alexandria LANGUAGES CXX)

add_definitions(-Wfatal-errors)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

add_subdirectory("deps/leveldb")
add_subdirectory("deps/abseil-cpp")

find_package(Threads REQUIRED)
FIND_PACKAGE(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system iostreams filesystem unit_test_framework)
find_package(ZLIB)
find_package(fcgi)

include_directories(src/)
include_directories(deps/)
include_directories(tests/)

set(SRC_CLASSES
	
	"src/api/Api.cpp"
	"src/api/ApiResponse.cpp"
	"src/api/ApiStatusResponse.cpp"
	"src/api/ResultWithSnippet.cpp"
	"src/api/LinkResult.cpp"
	"src/api/DomainLinkResult.cpp"
	"src/api/Worker.cpp"

	"src/file/File.cpp"
	"src/file/TsvFile.cpp"
	"src/file/GzTsvFile.cpp"
	"src/file/TsvFileRemote.cpp"
	"src/file/TsvRow.cpp"

	"src/transfer/Transfer.cpp"

	"src/full_text/FullTextIndexer.cpp"
	"src/full_text/FullTextIndexerRunner.cpp"
	"src/full_text/UrlToDomain.cpp"
	"src/full_text/FullText.cpp"

	"src/search_engine/SearchEngine.cpp"
	"src/search_engine/SearchAllocation.cpp"

	"src/link/Link.cpp"
	"src/link/Indexer.cpp"
	"src/link/IndexerRunner.cpp"
	"src/link/LinkCounter.cpp"

	"src/domain_link/Indexer.cpp"

	"src/hash_table/HashTable.cpp"
	"src/hash_table/HashTableShard.cpp"
	"src/hash_table/HashTableShardBuilder.cpp"
	"src/hash_table/HashTableHelper.cpp"
	"src/hash_table/builder.cpp"

	"src/post_processor/PostProcessor.cpp"
	
	"src/parser/Parser.cpp"
	"src/parser/entities.cpp"
	"src/parser/HtmlLink.cpp"
	"src/parser/HtmlParser.cpp"
	"src/parser/Unicode.cpp"
	"src/parser/URL.cpp"
	"src/parser/Warc.cpp"
	"src/parser/cc_parser.cpp"

	"src/urlstore/UrlStore.cpp"
	"src/urlstore/UrlData.cpp"
	"src/urlstore/DomainData.cpp"
	"src/urlstore/RobotsData.cpp"
	
	"src/system/Profiler.cpp"
	"src/system/System.cpp"
	"src/system/SubSystem.cpp"
	"src/system/Logger.cpp"
	"src/system/datetime.cpp"

	"src/utils/thread_pool.cpp"

	"src/memory/memory.cpp"

	"src/sort/Sort.cpp"

	"src/hash/Hash.cpp"

	"src/config.cpp"

	"src/KeyValueStore.cpp"

	"src/algorithm/Algorithm.cpp"
	"src/algorithm/HyperBall.cpp"

	"src/tools/Splitter.cpp"
	"src/tools/Counter.cpp"
	"src/tools/Download.cpp"
	"src/tools/CalculateHarmonic.cpp"
	"src/tools/generate_url_lists.cpp"

	"src/cluster/Document.cpp"
	"src/scraper/scraper.cpp"
	"src/scraper/store.cpp"

	"src/indexer/level.cpp"
	"src/indexer/snippet.cpp"
	"src/indexer/index_tree.cpp"
	"src/indexer/console.cpp"
	"src/indexer/merger.cpp"

	"src/domain_stats/domain_stats.cpp"

	"deps/robots.cc"
)

set(SRC_COMMON
	"src/common/Dictionary.cpp"
	"src/common/DictionaryRow.cpp"
	"src/text/Stopwords.cpp"
	"src/text/Text.cpp"
)

add_executable(run_tests
	"tests/main.cpp"
	"src/memory/debugger.cpp"
	${SRC_CLASSES}
	${SRC_COMMON}
)
add_executable(server
	"src/server.cpp"
	${SRC_CLASSES}
	${SRC_COMMON}
)
add_executable(scraper
	"src/scraper.cpp"
	${SRC_CLASSES}
	${SRC_COMMON}
)
add_executable(indexer
	"src/indexer.cpp"
	"src/memory/debugger.cpp"
	${SRC_CLASSES}
	${SRC_COMMON}
)

target_compile_definitions(run_tests PUBLIC CC_TESTING)
target_compile_definitions(run_tests PUBLIC FT_NUM_SHARDS=16)
target_compile_definitions(run_tests PUBLIC HT_NUM_SHARDS=16)
target_compile_definitions(run_tests PUBLIC FILE_SERVER="http://127.0.0.1")
target_compile_definitions(run_tests PUBLIC COMPILE_WITH_LINK_INDEX)

target_compile_options(run_tests PUBLIC -Wall -Werror)
target_compile_options(server PUBLIC -Wall -Werror)
target_compile_options(scraper PUBLIC -Wall -Werror)
target_compile_options(indexer PUBLIC -Wall -Werror)

target_link_libraries(run_tests PUBLIC
	${FCGI_LIBRARY}
	${FCGI_LIBRARYCPP}
	${CURL_LIBRARIES}
	${Boost_LIBRARIES} ZLIB::ZLIB Threads::Threads leveldb absl::strings absl::numeric)
target_link_libraries(server PUBLIC
	${FCGI_LIBRARY}
	${FCGI_LIBRARYCPP}
	${CURL_LIBRARIES}
	${Boost_LIBRARIES} ZLIB::ZLIB Threads::Threads leveldb absl::strings absl::numeric)
target_link_libraries(scraper PUBLIC
	${FCGI_LIBRARY}
	${FCGI_LIBRARYCPP}
	${CURL_LIBRARIES}
	${Boost_LIBRARIES} ZLIB::ZLIB Threads::Threads leveldb absl::strings absl::numeric)
target_link_libraries(indexer PUBLIC
	${FCGI_LIBRARY}
	${FCGI_LIBRARYCPP}
	${CURL_LIBRARIES}
	${Boost_LIBRARIES} ZLIB::ZLIB Threads::Threads leveldb absl::strings absl::numeric)

