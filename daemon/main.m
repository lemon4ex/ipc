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

static void daemon_peer_event_handler(xpc_connection_t peer, xpc_object_t event)
{
	xpc_type_t type = xpc_get_type(event);
	if (type == XPC_TYPE_ERROR) {
		if (event == XPC_ERROR_CONNECTION_INVALID) {
			// The client process on the other end of the connection has either
			// crashed or cancelled the connection. After receiving this error,
			// the connection is in an invalid state, and you do not need to
			// call xpc_connection_cancel(). Just tear down any associated state
			// here.
		} else if (event == XPC_ERROR_TERMINATION_IMMINENT) {
			// Handle per-connection termination cleanup.
		}
	} else {
		assert(type == XPC_TYPE_DICTIONARY);
		// Handle the message.
	}
}

static void daemon_event_handler(xpc_connection_t peer)
{
	// By defaults, new connections will target the default dispatch concurrent queue.
	xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
		daemon_peer_event_handler(peer, event);
	});
	
	// This will tell the connection to begin listening for events. If you
	// have some other initialization that must be done asynchronously, then
	// you can defer this call until after that initialization is done.
	xpc_connection_resume(peer);
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
        NSLog(@"usage：daemon [-s|-c]");
        return 0;
    }
    if (!strcmp(argv[1], "-s")) {
        xpc_connection_t service = xpc_connection_create_mach_service("net.ymlab.dev.daemon",
                                                                      dispatch_get_main_queue(),
                                                                      XPC_CONNECTION_MACH_SERVICE_LISTENER);
        
        if (!service) {
            NSLog(@"Failed to create service.");
            exit(EXIT_FAILURE);
        }
        
        xpc_connection_set_event_handler(service, ^(xpc_object_t connection) {
            daemon_event_handler(connection);
        });
        
        xpc_connection_resume(service);
        
    }else if(!strcmp(argv[1], "-c")){
        
    }
	dispatch_main();
    return EXIT_SUCCESS;
}
