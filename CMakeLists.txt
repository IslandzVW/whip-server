PROJECT( whip )
cmake_minimum_required(VERSION 2.6)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package( Threads )
find_package(Boost COMPONENTS thread system program_options filesystem date_time REQUIRED)

add_definitions("-DBOOST_ALL_DYN_LINK")
add_definitions("-DBOOST_ALL_NO_LIB")

include_directories( ${Boost_INCLUDE_DIR} "ooid" "sqlite")
set(LIBS ${LIBS} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_DATE_TIME_LIBRARY})

set ( Whip_srcs whip/AppLog.cpp whip/Asset.cpp whip/AssetCache.cpp whip/AssetClient.cpp whip/AssetSearch.cpp whip/AssetServer.cpp whip/AssetStorageError.cpp whip/AuthChallengeMsg.cpp whip/AuthResponseMsg.cpp whip/AuthStatusMsg.cpp whip/ClientRequestMsg.cpp whip/CSAuthenticated.cpp whip/CSUnauthenticated.cpp whip/DatabaseSet.cpp whip/DataFile.cpp whip/EndOfFileError.cpp whip/ExistenceIndex.cpp whip/IAsyncStorageBackend.cpp whip/IConnectionState.cpp whip/IDataFile.cpp whip/IIndexFileImplementation.cpp whip/IIndexFileManager.cpp whip/IndexFile.cpp whip/IndexFileError.cpp whip/IntraMeshMsg.cpp whip/IntraMeshQueryClient.cpp whip/IntraMeshService.cpp whip/IStorageBackend.cpp whip/MeshQueryConnection.cpp whip/MeshServer.cpp whip/MeshStorageBackend.cpp whip/PosixDataFileImpl.cpp whip/PullReplicationService.cpp whip/PushReplicationService.cpp whip/Random.cpp whip/RemoteAssetService.cpp whip/RequestError.cpp whip/ServerResponseMsg.cpp whip/Settings.cpp whip/SHA1.cpp whip/SQLiteConnection.cpp whip/SQLiteError.cpp whip/SQLiteIndexFile.cpp whip/SQLiteIndexFileManager.cpp whip/SQLiteTransaction.cpp whip/Validator.cpp whip/VFSBackend.cpp whip/VFSDatabase.cpp whip/VFSQueueTypes.cpp whip/whip.cpp whip/WinDataFileImpl.cpp whip/MovingAverage.cpp )

add_subdirectory( sqlite )

ADD_EXECUTABLE( whip ${Whip_srcs})
target_link_libraries(whip ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${LIBS} sqlite3 )
