//
//  main.m
//  daemon
//
//  Created by h4ck on 2020/11/13.
//  Copyright (c) 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

// XPC Service: Lightweight helper tool that performs work on behalf of an application.
// see http://developer.apple.com/library/mac/#documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingXPCServices.html

#include <xpc_lite/xpc_lite.h>
#include <Foundation/Foundation.h>
#include <sys/stat.h>

static void daemon_peer_event_handler(xpc_lite_connection_t peer, xpc_lite_object_t event)
{
	xpc_lite_type_t type = xpc_lite_get_type(event);
	if (type == XPC_TYPE_ERROR) {
		if (event == XPC_ERROR_CONNECTION_INVALID) {
			// The client process on the other end of the connection has either
			// crashed or cancelled the connection. After receiving this error,
			// the connection is in an invalid state, and you do not need to
			// call xpc_connection_cancel(). Just tear down any associated state
			// here.
		}
	} else {
		assert(type == XPC_TYPE_DICTIONARY);
		// Handle the message.
        double value1 = xpc_lite_dictionary_get_double(event, "value1");
        double value2 = xpc_lite_dictionary_get_double(event, "value2");
        xpc_lite_object_t dictionary = xpc_lite_dictionary_create(NULL, NULL, 0);
        xpc_lite_dictionary_set_double(dictionary, "result", value1+value2);
        xpc_lite_connection_send_message(peer, dictionary);
        xpc_lite_release(dictionary);
	}
}

static void daemon_event_handler(xpc_lite_connection_t peer)
{
	// By defaults, new connections will target the default dispatch concurrent queue.
	xpc_lite_connection_set_event_handler(peer, ^(xpc_lite_object_t event) {
		daemon_peer_event_handler(peer, event);
	});
	
	// This will tell the connection to begin listening for events. If you
	// have some other initialization that must be done asynchronously, then
	// you can defer this call until after that initialization is done.
	xpc_lite_connection_resume(peer);
}

#define CONNECTION_TYPE 0 // 0：UDS，1：TCP

#define SOCKET_DIR "/var/run/xpc"

int main(int argc, const char *argv[])
{
    if (argc < 2) {
        NSLog(@"usage：daemon [-s|-c]");
        return 0;
    }
    if (!strcmp(argv[1], "-s")) {
//        if (access(SOCKET_DIR, F_OK)) {
//            mkdir(SOCKET_DIR, S_IRWXO | S_IRWXU | S_IRWXG);
//        }
        // SOCKET_DIR"/net.ymlab.dev.daemon"
#if CONNECTION_TYPE == 0
        xpc_lite_connection_t service = xpc_lite_connection_create_mach_service(SOCKET_DIR"/net.ymlab.dev.daemon", dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
#else
        xpc_lite_connection_t service = xpc_lite_connection_create_tcp_service("127.0.0.1", 8998 , dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
#endif
        if (!service) {
            NSLog(@"Failed to create service.");
            exit(EXIT_FAILURE);
        }
        
        xpc_lite_connection_set_event_handler(service, ^(xpc_lite_object_t connection) {
            daemon_event_handler(connection);
        });
        
        xpc_lite_connection_resume(service);
        
    }else if(!strcmp(argv[1], "-c")){
        static xpc_lite_connection_t client = NULL;
#if CONNECTION_TYPE == 0
        client = xpc_lite_connection_create_mach_service(SOCKET_DIR"/net.ymlab.dev.daemon", dispatch_get_main_queue(), 0);
#else
        client = xpc_lite_connection_create_tcp_service("127.0.0.1", 8998 , dispatch_get_main_queue(), 0);
#endif
        xpc_lite_connection_set_event_handler(client, ^(xpc_lite_object_t object){
            double result = xpc_lite_dictionary_get_double(object, "result");
            NSLog(@"%f",result);
        });
        xpc_lite_connection_resume(client);
        static dispatch_source_t timer;
        static xpc_lite_object_t dictionary;
        timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
        dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC, 0 * NSEC_PER_SEC);
        dispatch_source_set_event_handler(timer, ^{
            dictionary = xpc_lite_dictionary_create(NULL, NULL, 0);
            xpc_lite_dictionary_set_double(dictionary, "value1", 1.0);
            xpc_lite_dictionary_set_double(dictionary, "value2", 2.0);
            xpc_lite_connection_send_message(client, dictionary);
            xpc_lite_release(dictionary);
        });
        dispatch_resume(timer);
    }
	dispatch_main();
    return EXIT_SUCCESS;
}
