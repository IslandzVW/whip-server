set ( Sqlite_srcs sqlite3.c )
add_library ( sqlite3 ${Sqlite_srcs} )

if (${UNIX})
target_link_libraries(sqlite3 PUBLIC dl)
endif (${UNIX})
