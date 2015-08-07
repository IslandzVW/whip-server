#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/any.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

#include <map>
#include <vector>

#include "sqlite3.h"

namespace iwvfs
{
	typedef std::map<std::string, boost::any> DBRow;
	typedef boost::shared_ptr<DBRow> DBRowPtr;
	typedef std::vector<DBRowPtr> DBResultSet;
	typedef boost::shared_ptr<DBResultSet> DBResultSetPtr;
	
	struct ColumnDef
	{
		std::string Name;
		int Type;
	};

	typedef std::vector<ColumnDef> ColumnDefList;

	/**
		A wrapper to a connection to an sqlite database
	*/
	class SQLiteConnection : public boost::noncopyable
	{
	public:
		typedef boost::shared_ptr<SQLiteConnection> ptr;

	private:
		sqlite3* _database;
		
		void throwOnError(const std::string& preamble, int result, std::vector<int> okCodes, bool closeConnection = false);
		
		/**
			Returns a list of column information for the given query
		*/
		ColumnDefList getColumnDefinitions(sqlite3_stmt* stmt);
		
		/**
			Extracts a row from the current statement
		*/
		DBRowPtr extractRow(sqlite3_stmt* stmt, const ColumnDefList& columnDefs);

	public:
		/**
			Opens the database on the given path
		*/
		SQLiteConnection(const boost::filesystem::path& path);
		virtual ~SQLiteConnection();
		
		/**
			Performs a query on the open connection and returns the results 
		*/
		DBResultSetPtr queryWithResults(const std::string& sql);

		/**
			Performs a query on the open connection that will return no results (insert, update, delete)
		*/
		void queryWithNoResults(const std::string& sql);

		/**
			Performs a query on the open connection and calls a callback function for each row.
			This interface is more difficult to use but is more efficient than 
		*/
		void queryWithRowCallback(const std::string& sql, boost::function<void(sqlite3_stmt*)> callBack);
	};
}
