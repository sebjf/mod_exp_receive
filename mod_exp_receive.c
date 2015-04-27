
#include <httpd.h>
#include <http_protocol.h>
#include <http_config.h>

/*
 * To develop this module with Eclipse, Use the New Project -> Existing Code With Makefile option and point it to the location of this file.
 * 		- Make sure to add the directory '/usr/include/httpd/' to the Include directories so the Eclipse autocomplete will work.
 * 		- To debug, create a new Debug Configuration of the type Application. Specify the application as '/usr/sbin/httpd' with the argument '-X'.
 * 			(Be aware that breakpoints may need to be added each time after httpd loads!)
 */

static int example_handler(request_rec *r)
{
    /* First off, we need to check if this is a call for the "example-handler" handler.
     * If it is, we accept it and do our things, if not, we simply return DECLINED,
     * and the server will try somewhere else.
     */
    if (!r->handler || strcmp(r->handler, "example-handler")) return (DECLINED);

    /* Now that we are handling this request, we'll write out "Hello, world!" to the client.
     * To do so, we must first set the appropriate content type, followed by our output.
     */
    ap_set_content_type(r, "text/html");
    ap_rprintf(r, "Hello, world!");

    /* Lastly, we must tell the server that we took care of this request and everything went fine.
     * We do so by simply returning the value OK to the server.
     */
    return OK;
}

static void register_hooks(apr_pool_t *pool)
{
    /* Create a hook in the request handler, so we get called when a request arrives */
    ap_hook_handler(example_handler, NULL, NULL, APR_HOOK_LAST);
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


