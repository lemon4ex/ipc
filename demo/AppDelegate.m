//
//  AppDelegate.m
//  demo
//
//  Created by h4ck on 2020/11/13.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#import "AppDelegate.h"
#include <ipc/ipc.h>
#include <Foundation/Foundation.h>

static void daemon_peer_event_handler(ipc_connection_t peer, ipc_object_t event)
{
    ipc_type_t type = ipc_get_type(event);
    if (type == XPC_TYPE_ERROR) {
        if (event == XPC_ERROR_CONNECTION_INVALID) {
            // The client process on the other end of the connection has either
            // crashed or cancelled the connection. After receiving this error,
            // the connection is in an invalid state, and you do not need to
            // call ipc_connection_cancel(). Just tear down any associated state
            // here.
            NSLog(@"connection closed");
        }
    } else {
        assert(type == XPC_TYPE_DICTIONARY);
        // Handle the message.
        double value1 = ipc_dictionary_get_double(event, "value1");
        double value2 = ipc_dictionary_get_double(event, "value2");
        ipc_object_t dictionary = ipc_dictionary_create(NULL, NULL, 0);
        ipc_dictionary_set_double(dictionary, "result", value1+value2);
        ipc_connection_send_message(peer, dictionary);
        ipc_release(dictionary);
    }
}

static void daemon_event_handler(ipc_connection_t peer)
{
    // By defaults, new connections will target the default dispatch concurrent queue.
    ipc_connection_set_event_handler(peer, ^(ipc_object_t event) {
        daemon_peer_event_handler(peer, event);
    });
    
    // This will tell the connection to begin listening for events. If you
    // have some other initialization that must be done asynchronously, then
    // you can defer this call until after that initialization is done.
    ipc_connection_resume(peer);
}

@interface AppDelegate ()
{
}
@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
    // net.ymlab.dev.daemon
    static ipc_connection_t service = NULL;
//    NSString *name = [NSSearchPathForDirectoriesInDomains(9, 1, 1)[0] stringByAppendingPathComponent:@"daemon"];
//    service = ipc_connection_create_uds_service(name.UTF8String, dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
    service = ipc_connection_create_tcp_service("127.0.0.1", 8998 , dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);

    if (!service) {
        NSLog(@"Failed to create service.");
        exit(EXIT_FAILURE);
    }

    ipc_connection_set_event_handler(service, ^(ipc_object_t connection) {
        daemon_event_handler((ipc_connection_t)connection);
    });

    ipc_connection_resume(service);
    
    static ipc_connection_t client = NULL;
//    client = ipc_connection_create_uds_service(name.UTF8String, dispatch_get_main_queue(), 0);
    client = ipc_connection_create_tcp_service("127.0.0.1", 8998 , dispatch_get_main_queue(), 0);
    ipc_connection_set_event_handler(client, ^(ipc_object_t object){
        double result = ipc_dictionary_get_double(object, "result");
        NSLog(@"%f",result);
    });
    ipc_connection_resume(client);

    static ipc_object_t dictionary;
    static dispatch_source_t timer;
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC, 0 * NSEC_PER_SEC);
    dispatch_source_set_event_handler(timer, ^{
        dictionary = ipc_dictionary_create(NULL, NULL, 0);
        ipc_dictionary_set_double(dictionary, "value1", 1.0);
        ipc_dictionary_set_double(dictionary, "value2", 2.0);
        ipc_connection_send_message(client, dictionary);
        ipc_release(dictionary);
    });
    dispatch_resume(timer);
//    dictionary = ipc_dictionary_create(NULL, NULL, 0);
//    ipc_dictionary_set_double(dictionary, "value1", 1.0);
//    ipc_dictionary_set_double(dictionary, "value2", 2.0);
//    ipc_connection_send_message_with_reply(client, dictionary, NULL, ^(ipc_object_t object) {
//        double result = ipc_dictionary_get_double(object, "result");
//        NSLog(@"%f",result);
//    });
//    ipc_release(dictionary);
    
    return YES;
}


#pragma mark - UISceneSession lifecycle


- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options {
    // Called when a new scene session is being created.
    // Use this method to select a configuration to create the new scene with.
    return [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
}


- (void)application:(UIApplication *)application didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions {
    // Called when the user discards a scene session.
    // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
    // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
}


@end
