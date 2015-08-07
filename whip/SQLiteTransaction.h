#pragma once

#include "SQLiteConnection.h"

namespace iwvfs
{
	/**
		Manages beginning and end of transactions.  Rolls back a transaction if destroyed before calling commit
	*/
	class SQLiteTransaction
	{
	private:
		bool _committed;
		SQLiteConnection::ptr _dbConn;

	public:
		SQLiteTransaction(SQLiteConnection::ptr dbConn);
		virtual ~SQLiteTransaction();

		/**
			Commits the transaction to the database
		*/
		void commit();
	};
}
