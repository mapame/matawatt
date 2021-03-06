#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "common.h"
#include "logger.h"
#include "config.h"
#include "power.h"
#include "database.h"

int energy_add_power(power_data_t *pd) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_store_minute[] = "INSERT INTO energy_minutes(timestamp,latest_second,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6)"
									" ON CONFLICT(timestamp) DO UPDATE SET second_count = second_count + 1, latest_second = excluded.latest_second, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost WHERE latest_second < excluded.latest_second;";
	const char sql_store_hour[] = "INSERT INTO energy_hours(year,month,day,hour,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)"
									" ON CONFLICT(year,month,day,hour) DO UPDATE SET second_count = second_count + 1, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost;";
	const char sql_store_day[] = "INSERT INTO energy_days(year,month,day,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6,?7)"
									" ON CONFLICT(year,month,day) DO UPDATE SET second_count = second_count + 1, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost;";
	time_t timestamp_minute;
	struct tm time_tm;
	int year, month, day, hour;
	double p_total;
	double active_energy_total;
	double reactive_energy_total;
	double cost;
	
	if(pd == NULL)
		return -1;
	
	timestamp_minute = pd->timestamp - (pd->timestamp % 60);
	localtime_r(&(pd->timestamp), &time_tm);
	year = time_tm.tm_year + 1900;
	month = time_tm.tm_mon + 1;
	day = time_tm.tm_mday;
	hour = time_tm.tm_hour;
	
	p_total = pd->p[0] + pd->p[1];
	// Energia ativa em KWh e reativa em kvarh
	active_energy_total = p_total / (3600.0 * 1000.0);
	reactive_energy_total = (pd->q[0] + pd->q[1]) / (3600.0 * 1000.0);
	
	cost = config_get_value_double("kwh_rate", 0, 10, 0) * active_energy_total;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if(sqlite3_exec(db_conn, "BEGIN TRANSACTION", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERROR("Failed to begin SQL transaction: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Minuto
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_minute, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for minute power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int64(ppstmt, 1, timestamp_minute);
	result += sqlite3_bind_int64(ppstmt, 2, pd->timestamp);
	result += sqlite3_bind_double(ppstmt, 3, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 4, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 5, p_total);
	result += sqlite3_bind_double(ppstmt, 6, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as minute: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Hora
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_hour, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for hour power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, year);
	result += sqlite3_bind_int(ppstmt, 2, month);
	result += sqlite3_bind_int(ppstmt, 3, day);
	result += sqlite3_bind_int(ppstmt, 4, hour);
	result += sqlite3_bind_double(ppstmt, 5, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 6, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 7, p_total);
	result += sqlite3_bind_double(ppstmt, 8, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as hour: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Dia
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_day, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for day power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, year);
	result += sqlite3_bind_int(ppstmt, 2, month);
	result += sqlite3_bind_int(ppstmt, 3, day);
	result += sqlite3_bind_double(ppstmt, 4, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 5, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 6, p_total);
	result += sqlite3_bind_double(ppstmt, 7, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as day: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if(sqlite3_exec(db_conn, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERROR("Failed to commit power data to database: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_close(db_conn);
	
	return 0;
}
