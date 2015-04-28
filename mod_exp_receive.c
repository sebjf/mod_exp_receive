
#include "ap_config.h"
#include "ap_provider.h"
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"

#define EXP_DATA_MIMETYPE	"application/prs.exp"
#define DIRECTORY			"/logs/"
#define USERIDFIELD			"From2"

/*
 * To develop this module with Eclipse, Use the New Project -> Existing Code With Makefile option and point it to the location of this file.
 * 		- Make sure to add the directories '/usr/include/httpd/', '/usr/include/apr-1' to the Include directories so the Eclipse autocomplete will work.
 * 		- To debug, create a new Debug Configuration of the type Application. Specify the application as '/usr/sbin/httpd' with the argument '-X'.
 * 			(Be aware that breakpoints may need to be added each time after httpd loads!)
 */

struct experiment_record
{
	char* participant_identifier;

};


static int process_request_body(request_rec *r, const char* body, int body_size)
{
	const char* from_data = apr_table_get(r->headers_in, USERIDFIELD);

	char* filename = apr_pcalloc(r->pool, strlen(DIRECTORY) + strlen(from_data));
	strcpy(filename, DIRECTORY);
	strcat(filename, from_data);

	FILE *fp = fopen(filename, "w+");

	if(fp == NULL)
	{
		ap_log_rerror("mod_exp_receive.c", 41, APLOG_ERR, r->status, r, "Could not open file: %s", strerror(errno));
		return OK;
	}

	fwrite(body, 1, body_size, fp);
	fclose(fp);

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
			//return DECLINED;
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

	//Check that the received data is the same length as the reported content length
	if(content_length_data){
		if(body_size != strtol(content_length_data, NULL, 0))
		{
			const char* from_data = apr_table_get(r->headers_in, USERIDFIELD);
			ap_log_rerror("mod_exp_receive.c", 133, APLOG_WARNING, r->status, r, "The number of bytes received in the" \
					" body from client %s differs from the specified Content-Length (%s, %i)", from_data, content_length_data, body_size);
		}
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
    ap_rprintf(r, "Success.");

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


