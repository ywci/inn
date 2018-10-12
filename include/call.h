/******************* ADD YOUR CALLBACK FUNCTION HERE ********************
 * This callback function is invoked in the course of request delivery.
 * The following is an example of the callback function for each request.
 *
 * Parameters:
 *   1) buf: the data of the request
 *   2) size: the size of the request
 ***********************************************************************/

inline void call(char *buf, size_t size)
{
    /* Example ----------------- */
#ifdef EVALUATE
    evaluate(buf, size);
#endif
    /* ------------------------- */
}
