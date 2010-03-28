//
//  RhoFbConnect.m
//  FBConnect
//
//  Created by Vlad on 8/7/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import "FBConnect/FBConnect.h"
#import "RhoFbConnect.h"
#import "syscall.h"
#import "dosyscall.h"

static RhoFbConnect* sharedInstance = nil;

static NSString* strApiKey = @"xxx";
static NSString* strSecret = @"xxx";

@implementation RhoFbConnect

- (NSString*)normalizeUrl:(NSString*)url {
	if([url hasPrefix:@"http://"]) {
		return url;
	}
	return [@"http://localhost:8080/" stringByAppendingPathComponent:url];
}

- (void)NotifyViewThreadRoutine:(id)object {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
	// Get message body and its length
	NSData *postBody = (NSData*)object;	
	NSString *postLength = [NSString stringWithFormat:@"%d", [postBody length]];
	
	// Create post request
	NSMutableURLRequest *request = [[NSMutableURLRequest alloc] init];
	NSString* url = [self normalizeUrl:loginCallback];
	NSLog(@"Callback url: %@",url);	
	[request setURL:[NSURL URLWithString:url]];
	[request setHTTPMethod:@"POST"];
	[request setValue:postLength forHTTPHeaderField:@"Content-Length"];
	[request setHTTPBody:postBody];
	
	// Send request
	[NSURLConnection sendSynchronousRequest:request returningResponse:nil error:nil];
	
    [pool release];
}

- (void)doCallback:(NSString*) message {
	// Create post body
	NSData* postBody = [message dataUsingEncoding:NSUTF8StringEncoding];
	// Start notification thread	
	[NSThread detachNewThreadSelector:@selector(NotifyViewThreadRoutine:)
							 toTarget:self withObject:postBody];		
}

- (void) login:(NSString*)callback {
	loginCallback = [[NSString alloc] initWithString:callback];
	FBSession* s = [FBSession sessionForApplication:strApiKey secret:strSecret delegate:self];
	if ([s resume]) {
		[self session:s didLogin:s.uid];
	} else {
		FBLoginDialog* dialog = [[[FBLoginDialog alloc] initWithSession:s] autorelease]; 
		[dialog show];
	}
}

- (void)session:(FBSession*)session didLogin:(FBUID)uid {
	NSString* message = @"status=login-successfully";
	message = [message stringByAppendingString:@"&session-key="];
	message = [message stringByAppendingString:session.sessionKey];
	[self doCallback:message];
}

- (void)session:(FBSession*)session willLogout:(FBUID)uid {
	[self doCallback:@"status=will-logout"];
}

- (void)sessionDidLogout:(FBSession*)session {
	[self doCallback:@"status=logout-successfully"];
}

// singlton

- (id) init {
	self = [super init];
	if (self != nil) {
		//self.sessionCallback = nil;
	}
	return self;
}

- (void)dealloc 
{
    [loginCallback release];
	[super dealloc];
}

+ (RhoFbConnect *)sharedInstance {
    @synchronized(self) {
        if (sharedInstance == nil) {
            [[self alloc] init]; // assignment not done here
        }
    }
    return sharedInstance;
}

+ (id)allocWithZone:(NSZone *)zone {
    @synchronized(self) {
        if (sharedInstance == nil) {
            sharedInstance = [super allocWithZone:zone];
            return sharedInstance;  // assignment and return on first allocation
        }
    }
    return nil; // on subsequent allocation attempts return nil
}

- (id)copyWithZone:(NSZone *)zone
{
    return self;
}

- (id)retain {
    return self;
}

- (unsigned)retainCount {
    return UINT_MAX;  // denotes an object that cannot be released
}

- (void)release {
    //do nothing
}

- (id)autorelease {
    return self;
}

@end

PARAMS_WRAPPER* fb_do_syscall(PARAMS_WRAPPER* params) {
	int i;
	
	//Look through input parameters
	if (params) {
		printf("Calling \"%s\" with parameters:\n", params->_callname);
		if (strcmp("login",params->_callname) == 0) {
			for (i=0;i<params->_nparams;i++) {
				printf("%s => %s\n",params->_names[i],params->_values[i]);
				if (strcmp("callback",params->_names[i])==0) {
					NSString* callback = [[NSString alloc] initWithCString:params->_values[i] 
															 encoding:[NSString defaultCStringEncoding]];
					[[RhoFbConnect sharedInstance] login:callback];
					break;
				}
			}
		}
	}
	return NULL;
}

