
#include <stdio.h>
#include <uuid/uuid.h>
#include "ap_config.h"
#include "ap_provider.h"
#include "apr.h"
#include "apr_file_io.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"

#define EXP_DATA_MIMETYPE	"application/prs.exp"
#define LOGFILENAMEPREFIX	"/logs/log_"
#define USERIDFIELD			"From2"

//https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/uuid_unparse_lower.3.html
#define UUIDSTRLEN			37



/*
 * To develop this module with Eclipse, Use the New Project -> Existing Code With Makefile option and point it to the location of this file.
 * 		- Make sure to add the directories '/usr/include/httpd/', '/usr/include/apr-1' to the Include directories so the Eclipse autocomplete will work.
 * 		- To debug, create a new Debug Configuration of the type Application. Specify the application as '/usr/sbin/httpd' with the argument '-X'.
 * 			(Be aware that breakpoints may need to be added each time after httpd loads!)
 */

/* Each request gets its own file - generate a random, unused filename and open it */
/* Use fopen() to test for file existance so if it succeeds (in finding an unused name) there is no race condition between this and opening it */
static apr_file_t* get_request_file(request_rec* r)
{
	apr_file_t* fp = NULL;

	char* filename = apr_pcalloc(r->pool, strlen(LOGFILENAMEPREFIX) + UUIDSTRLEN);
	strcpy(filename, LOGFILENAMEPREFIX);

	apr_status_t result = APR_EEXIST;
	do{
		uuid_t uu;
		uuid_generate(uu);
		uuid_unparse_lower(uu, filename + strlen(LOGFILENAMEPREFIX));
		result = apr_file_open(&fp, filename, (APR_WRITE | APR_CREATE | APR_EXCL), APR_OS_DEFAULT, r->pool);
	}
	while(result == APR_EEXIST);

	if(result != OK)
	{
		ap_log_rerror("mod_exp_receive.c", 35, APLOG_ERR, r->status, r, "Could not open file: %s [%i]. Have you checked SELINUX context for the directory?", filename, result);
		return NULL;
	}

	return fp;
}

static int process_request_body(request_rec *r, const char* body, int body_size)
{
	apr_file_t* fp = get_request_file(r);

	if(fp == NULL)
	{
		return OK;
	}

	apr_size_t nbytes = body_size;
	apr_file_write(fp, body, &nbytes);
	apr_file_close(fp);

	return OK;
}

/*
 * Read all the request data from the client and store in a buffer for processing
 * http://docstore.mik.ua/orelly/apache_mod/139.htm
 */
static int read_request_data(request_rec *r, const char **rbuf, int* size) {

	int rc;
	if ((rc = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR)) != OK)
	{
		return rc;
	}

	if (ap_should_client_block(r))
	{
		char argsbuffer[HUGE_STRING_LEN];
		int num_bytes_to_copy, block_size, received_count = 0;
		long length = r->remaining;

		*rbuf = apr_palloc(r->pool, length + 1);

		while ((block_size = ap_get_client_block(r, argsbuffer, sizeof(argsbuffer))) > 0)
		{
			if ((received_count + block_size) > length)
			{
				num_bytes_to_copy = length - received_count;
			}
			else
			{
				num_bytes_to_copy = block_size;
			}

			memcpy((char*) *rbuf + received_count, argsbuffer, num_bytes_to_copy);
			received_count += num_bytes_to_copy;

			(*size) = received_count;
		}
	}

	return rc;
}


/*
 * Checks if the request is a valid experimental POST report, and if so reads and stores the report with read_request_data()
 * http://httpd.apache.org/docs/2.4/developer/modguide.html
 */
static int request_handler(request_rec *r)
{
    /* Check if we should handle this request, and return DECLINED if not. */

	int result = 0;

	//We only handle POST requests
	if(r->method_number != M_POST)
	{
		return DECLINED;
	}

	//Retrieve the headers
	const char* content_length_data = apr_table_get(r->headers_in, "Content-Length");
	const char* content_type_data = apr_table_get(r->headers_in, "Content-Type");

	//Check to make sure we are receiving experimental data
	if(content_type_data){
		if(strcmp(content_type_data,EXP_DATA_MIMETYPE))
		{
			return DECLINED;
		}
	}

	//Get the request body
	const char* body = NULL;
	int   body_size = 0;
	result = read_request_data(r, &body, &body_size);

	//If the request body has failed, return the error code
	if(result != OK)
	{
		return result;
	}

	//Process the request body
	result = process_request_body(r, body, body_size);

	if(result != OK)
	{
		return result;
	}


    /* Now that we are handling this request, we'll write out "Hello, world!" to the client.
     * To do so, we must first set the appropriate content type, followed by our output.
     */
    ap_set_content_type(r, "text/html");
    ap_rprintf(r, "Received.");

    /* Lastly, we must tell the server that we took care of this request and everything went fine.
     * We do so by simply returning the value OK to the server.
     */
    return OK;
}

static void register_hooks(apr_pool_t *pool)
{
    /* Create a hook in the request handler, so we get called when a request arrives */
    ap_hook_handler(request_handler, NULL, NULL, APR_HOOK_LAST);
}

module AP_MODULE_DECLARE_DATA   exp_receive_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    register_hooks   /* Our hook registering function */
};


