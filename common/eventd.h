#include "events_service.h"

typedef map<runtime_id_t, events_cache_type_t> last_events_t;

class eventd_server {
    public:
        /* Creates the zmq context */
        eventd_server();

        ~eventd_server();

        /*
         *  Main eventd service loop that honors event_req_type_t
         *
         *  For echo, it just echoes
         *
         *  FOr cache start, create the SUB end of capture and kick off
         *  capture_events thread. Upon cache stop command, close the handle
         *  which will stop the caching thread with read failure.
         *
         *  for cache read, returns the collected events as subset of 
         *  strings.
         *
         */
        int eventd_service();


        /*
         * For any fatal failure, terminate the entire run across threads
         * by deleting the zmq context.
         */
        void close();

    private:
        /*
         *  Started by eventd_service.
         *  Creates XPUB & XSUB end points.
         *  Bind the same
         *  Create a PUB socket end point for capture and bind.
         *  Call run_proxy method with sockets in a dedicated thread.
         *  Thread runs forever until the zmq context is terminated.
         */
        int zproxy_service();
        int zproxy_service_run(void *front, void *back, void *capture);


        /*
         *  Capture/Cache service
         *
         *  The service started in a dedicted thread upon demand.
         *  It expects SUB end of capture created & connected to the PUB
         *  capture end in zproxy service.
         *
         *  It goes in a forever loop, until the zmq read fails, which will happen
         *  if the capture/SUB end is closed. The stop cache service will close it,
         *  while start cache service creates & connects.
         *
         *  Hence this thread/function is active between cache start & stop.
         *
         *  Each event is 2 parts. It drops the first part, which is
         *  more for filtering events. It creates string from second part
         *  and saves it.
         *
         *  The string is the serialized version of internal_event_ref
         *
         *  It keeps two sets of data
         *      1) List of all events received in vector in same order as received
         *      2) Map of last event from each runtime id
         *
         *  We add to the vector as much as allowed by vector and as well
         *  the available memory. When mem exhausts, just keep updating map
         *  with last event from that sender. 
         *  
         *  The sequence number in map will help assess the missed count.
         *
         *  Thread is started upon creating SUB end of capture socket.
         */
        int capture_events();


    private:
        void *m_ctx;

        events_data_lst_t m_events;

        last_events_t m_last_events;

        void *m_capture;


        thread m_thread_proxy;
        thread m_thread_capture;
};








