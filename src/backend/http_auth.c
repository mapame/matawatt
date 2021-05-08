#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

#include "http.h"
#include "logger.h"
#include "auth.h"

unsigned int http_handler_auth_login(struct MHD_Connection *conn,
									int logged_user_id,
									path_parameter_t *path_parameters,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg) {
	
	json_object *top_json = NULL;
	json_object *username_json = NULL, *password_json = NULL;
	char *username = NULL, *password = NULL;
	int result;
	int user_id = 0;
	char *key = NULL;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	top_json = json_tokener_parse(req_data);
	
	if(json_object_object_get_ex(top_json, "username", &username_json) == 0 || json_object_object_get_ex(top_json, "password", &password_json) == 0) {
		json_object_put(top_json);
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if(json_object_get_type(username_json) != json_type_string || json_object_get_type(password_json) != json_type_string) {
		json_object_put(top_json);
		return MHD_HTTP_BAD_REQUEST;
	}
	
	username = strdup(json_object_get_string(username_json));
	password = strdup(json_object_get_string(password_json));
	
	json_object_put(top_json);
	
	result = auth_user_login(username, password, &user_id);
	
	free(username);
	free(password);
	
	if(result < 0)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	if(result > 0) {
		*resp_data = (char*) malloc(sizeof(char) * 40);
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"%s\"}", (result == 2) ? "inactive" : "wrong");
	} else {
		key = auth_new_session(user_id);
		
		if(key == NULL)
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		
		*resp_data = (char*) malloc(sizeof(char) * (40 + strlen(key)));
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"success\",\"access_key\":\"%s\"}", key);
	}
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_auth_verify(struct MHD_Connection *conn,
									int logged_user_id,
									path_parameter_t *path_parameters,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg) {
	
	*resp_data = (char*) malloc(sizeof(char) * 30);
	*resp_data_size = sprintf(*resp_data, "{\"result\":\"%s\"}", (logged_user_id > 0) ? "valid" : "invalid");
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}
