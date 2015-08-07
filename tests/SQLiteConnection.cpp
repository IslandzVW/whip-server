#include "StdAfx.h"
#include "SQLiteConnection.h"

#include <boost/assert.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include "SQLiteError.h"

using namespace boost::assign; // bring 'map_list_of()' into scope

namespace iwvfs
{
	SQLiteConnection::SQLiteConnection(const boost::filesystem::path& path)
	{
		int status = sqlite3_open_v2(path.string().c_str(), &_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
		this->throwOnError("[SQLiteConnection] Could not open database: ", status, list_of(SQLITE_OK), true);
	}

	SQLiteConnection::~SQLiteConnection()
	{
		sqlite3_close(_database);
	}

	void SQLiteConnection::throwOnError(const std::string& preamble, int result, std::vector<int> okCodes, bool closeConnection)
	{
		if (std::find(okCodes.begin(), okCodes.end(), result) == okCodes.end()) {
			std::string errMessage(preamble + sqlite3_errmsg(_database));
			int extendedError = sqlite3_extended_errcode(_database);

			if (closeConnection) {
				sqlite3_close(_database);
				_database = 0;
			}

			throw SQLiteError(errMessage, result, 0);
		}
	}

	ColumnDefList SQLiteConnection::getColumnDefinitions(sqlite3_stmt* stmt)
	{
		ColumnDefList columnDefs;
		int columnCount = sqlite3_column_count(stmt);

		for (int i = 0; i < columnCount; ++i) {
			ColumnDef def;

			def.Name = sqlite3_column_name(stmt, i);
			def.Type = sqlite3_column_type(stmt, i);

			columnDefs.push_back(def);
		}

		return columnDefs;
	}

	DBRowPtr SQLiteConnection::extractRow(sqlite3_stmt* stmt, const ColumnDefList& columnDefs)
	{
		DBRowPtr row(new DBRow());
		
		for (unsigned int i = 0; i < columnDefs.size(); ++i) {
			const ColumnDef& def = columnDefs[i];
			boost::any value;

			switch (def.Type) {
				case SQLITE_INTEGER:
					value = sqlite3_column_int64(stmt, i);
					break;

				case SQLITE_FLOAT:
					value = sqlite3_column_double(stmt, i);
					break;

				case SQLITE_TEXT:
					{
						std::string text((char*) sqlite3_column_text(stmt, i));
						value = text;
					}
					break;

				case SQLITE_NULL:
					//leave the value empty
					break;

				default:
					throw std::runtime_error("Unable to get value for column in database:  Type not supported");
			}

			(*row)[def.Name] = value;
		}

		return row;
	}

	/**
		Defines a custom deleter for an sqlite3_stmt object
	*/
	void stmt_deleter(sqlite3_stmt* stmt)
	{
		sqlite3_finalize(stmt);
	}

	DBResultSetPtr SQLiteConnection::queryWithResults(const std::string& sql)
	{
		sqlite3_stmt* stmt;
		int result = sqlite3_prepare_v2(_database, sql.c_str(), boost::numeric_cast<int>(sql.length()), &stmt, 0);
		this->throwOnError("[SQLiteConnection] Unable to prepare query: \"" + sql + "\": ", result, list_of(SQLITE_OK));

		//ensure that stmt will be freed even if exceptions are thrown
		boost::shared_ptr<sqlite3_stmt> stmtPtr(stmt, stmt_deleter);

		//prepare a result set
		DBResultSetPtr results(new DBResultSet());

		//retrieve type info for each row
		ColumnDefList columnDefs;

		//loop through the rows and create records
		while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
			if (columnDefs.size() == 0) {
				columnDefs = this->getColumnDefinitions(stmt);
				BOOST_ASSERT(columnDefs.size() > 0);
			}

			results->push_back(this->extractRow(stmt, columnDefs));
		}

		this->throwOnError("[SQLiteConnection] Unable to execute query: \"" + sql + "\": ", result, list_of((SQLITE_DONE)));

		return results;
	}

	void SQLiteConnection::queryWithNoResults(const std::string& sql)
	{
		sqlite3_stmt* stmt;
		int result = sqlite3_prepare_v2(_database, sql.c_str(), boost::numeric_cast<int>(sql.length()), &stmt, 0);
		this->throwOnError("[SQLiteConnection] Unable to prepare query: \"" + sql + "\": ", result, list_of(SQLITE_OK));

		//ensure that stmt will be freed even if exceptions are thrown
		boost::shared_ptr<sqlite3_stmt> stmtPtr(stmt, stmt_deleter);

		result = sqlite3_step(stmt);
		this->throwOnError("[SQLiteConnection] Unable to execute query: \"" + sql + "\": ", result, list_of(SQLITE_DONE));

	}

	void SQLiteConnection::queryWithRowCallback(const std::string& sql, boost::function<void(sqlite3_stmt*)> callBack)
	{
		sqlite3_stmt* stmt;
		int result = sqlite3_prepare_v2(_database, sql.c_str(), boost::numeric_cast<int>(sql.length()), &stmt, 0);
		this->throwOnError("[SQLiteConnection] Unable to prepare query: \"" + sql + "\": ", result, list_of(SQLITE_OK));

		//ensure that stmt will be freed even if exceptions are thrown
		boost::shared_ptr<sqlite3_stmt> stmtPtr(stmt, stmt_deleter);

		//loop through the rows and create records
		while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
			callBack(stmt);
		}

		this->throwOnError("[SQLiteConnection] Unable to execute query: \"" + sql + "\": ", result, list_of((SQLITE_DONE)));
	}
}