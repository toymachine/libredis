/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "alloc.h"
#include "parser.h"

#define MARK rp->mark = rp->p

struct _ReplyParser
{
    size_t p; //position
    int cs; //state
    int bulk_count; //number of chars to read for current binary safe bulk-value
    int multibulk_count; //the number of bulk replies to read for the current multibulk reply
    Reply *multibulk_reply;

    size_t mark; //helper to mark start of interesting data

};

void ReplyParser_reset(ReplyParser *rp)
{  
    rp->p = 0;
    rp->cs = 0;  
    rp->bulk_count = 0;
    rp->mark = 0;

    rp->multibulk_count = 0;
    rp->multibulk_reply = NULL;
}

ReplyParser *ReplyParser_new()
{
	DEBUG(("alloc ReplyParser\n"));
	ReplyParser *rp = Alloc_alloc_T(ReplyParser);
	if(rp == NULL) {
		Module_set_error(GET_MODULE(), "Out of memory while allocating ReplyParser");
		return NULL;
	}
	ReplyParser_reset(rp);
	return rp;
}

void ReplyParser_free(ReplyParser *rp)
{
	if(rp == NULL) {
		return;
	}
	DEBUG(("dealloc ReplyParser\n"));
	Alloc_free_T(rp, ReplyParser);
}


/**
 * A State machine for parsing Redis replies.
 * State is kept in the ReplyParser instance rp. The execute method can be called over and over
 * again parsing evermore Replies from the given buffer.
 * The method returns with RPR_ERROR if there is an error in the stream,
 * RPR_MORE if it is not in an end-state, but the buffer ran out, indicating that more
 * data needs to be read.
 * Finally it returns RPR_REPLY everytime a valid Redis reply is parsed from the buffer, returning an instance
 * of Reply in the 'reply' out parameter.
 * 0 is the initial state, and after reading a valid reply, the machine will return to this state, ready to parse
 * a new reply.
 * states: 0->1->2 => single line positive reply (+OK\r\n)
 * 		   0->3->4 => single line negative reply (-Some error msg\r\n)
 * 	       0->5->6->7->8 => nil bulk reply ($-1\r\n)
 * 		   0->5->9->10->11->12 => bulk reply ($5\r\nblaat\r\n)
 * 		   0->13->14->15->16 => nil multibulk reply (*-1\r\n)
 * 		   0->13->17->18 => multibulk reply (*3\r\n (... bulk replies ...)
 * 		   0->19->20 => integer reply (:42\r\n)
 * Note that it is not a 'pure' state machine (from a language theory perspective), e.g. some additional state is kept to
 * keep track of the number of chars to still read in a bulk reply, and some state to keep track of bulk replies that
 * belong to a multibulk reply.
 */
ReplyParserResult ReplyParser_execute(ReplyParser *rp, Buffer *buffer, size_t len, Reply **reply)
{    
	DEBUG(("enter rp exec, rp->p: %d, len: %d, cs: %d\n", rp->p, len, rp->cs));
	assert(rp->p <= len);
    while((rp->p) < len) {
    	*reply = NULL;
    	Byte c = Buffer_data(buffer)[rp->p];
        //printf("cs: %d, char: %d\n", rp->cs, c);
        switch(rp->cs) {
            case 0: { //initial state
                if(c == '$') { //possible start of bulk-reply
                    rp->p++;
                    rp->cs = 5;
                    continue;
                }
                else if(rp->multibulk_count == 0) { 
                	//the replies below only match when we are NOT in a multibulk reply.
                    if(c == '+') {
                    	//possible start of positive single line server reply (e.g. +OK\r\n)
                        rp->p++;
                        rp->cs = 1;
                        MARK;
                        continue;
                    }
                    else if(c == '-') { //negative
                    	//possible start of negative single line server reply (e.g. -Some error message\r\n)
                    	rp->p++;
                        rp->cs = 3;
                        MARK;
                        continue;
                    }
                    else if(c == '*') {
                    	//possible start of multibulk reply
                        rp->p++;
                        rp->cs = 13;
                        continue;
                    }
                    else if(c == ':') {
                    	//possible start of integer reply
                    	rp->p++;
                    	rp->cs = 19;
                    	MARK;
                    	continue;
                    }
                }
                break; 
            }
            //term CRLF of single line reply
            case 1: {
                if(c == CR) {
                    rp->p++;
                    rp->cs = 2;
                    continue;
                }
                else {
                    rp->p++;
                    continue;
                }
                break;
            }
            case 2: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 0;
                    //report line data
                    *reply = Reply_new(RT_OK, buffer, rp->mark, rp->p - rp->mark - 2);
                    return RPR_REPLY;
                }
                break;
            }
            //end term CRLF of single line reply

            //term CRLF of error line reply
            case 3: {
                if(c == CR) {
                    rp->p++;
                    rp->cs = 4;
                    continue;
                }
                else {
                    rp->p++;
                    continue;
                }
                break;
            }
            case 4: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 0;
                    //report error line data
                    *reply = Reply_new(RT_ERROR, buffer, rp->mark, rp->p - rp->mark - 2);
                    return RPR_REPLY;
                }
                break;
            }
            //end term CRLF of single line reply

            //start bulk reply
            case 5: {
                if(c == '-') { //nill bulk reply
                    rp->p++;
                    rp->cs = 6; 
                    continue;
                }
                else if(isdigit(c)) { //normal bulk reply
                    MARK;
                    rp->p++;
                    rp->cs = 9;
                    continue;
                }
                break;
            }
            //start nil bulk reply
            case 6: {
                if(c == '1') {
                    rp->p++;
                    rp->cs = 7;
                    continue;
                }
                break;
            }
            case 7: {
                if(c == CR) {
                    rp->p++;
                    rp->cs = 8;
                    continue;
                }
                break;
            }
            case 8: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 0;
                    *reply = Reply_new(RT_BULK_NIL, buffer, 0, 0);
                    if(rp->multibulk_count > 0) {
                        rp->multibulk_count -= 1;
                        assert(rp->multibulk_reply != NULL);
                        Reply_add_child(rp->multibulk_reply, *reply);
                        if(rp->multibulk_count == 0) {
                        	*reply = rp->multibulk_reply;
                        	rp->multibulk_reply = NULL;
                        	return RPR_REPLY;
                        }
                        else {
                        	continue;
                        }
                    }
                    else {
                        return RPR_REPLY;
                    }
                }
                break;
            }
            //end nil bulk reply
            //start normal bulk reply
            case 9: {
                if(c == CR) { //end of digits
                    rp->bulk_count = atoi(Buffer_data(buffer) + rp->mark);
                    rp->p++;
                    rp->cs = 10;
                    continue;
                }
                else if(isdigit(c)) { //one more digit
                    rp->p++;
                    continue;
                }
                break;
            }
            case 10: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 11;
                    MARK;
                    continue;
                }
                break;
            }
            case 11: { //reading of bulk_count chars    
                int n = MIN(rp->bulk_count, len - rp->p);
                //printf("n=%d\n", n);
                if(n == 0 && c == CR) {
                    rp->p++;
                    rp->cs = 12;
                    continue;
                }
                else if(n > 0) {
                    rp->p += n;
                    rp->bulk_count -= n;
                    continue;    
                }
                break;
            }
            case 12: {
                if(c == LF) {
                    assert(rp->bulk_count == 0);
                    rp->p++;
                    rp->cs = 0;
                    *reply = Reply_new(RT_BULK, buffer, rp->mark, rp->p - rp->mark - 2);
                    if(rp->multibulk_count > 0) {
                        rp->multibulk_count -= 1;
                        assert(rp->multibulk_reply != NULL);
                        Reply_add_child(rp->multibulk_reply, *reply);
                        if(rp->multibulk_count == 0) {
                        	*reply = rp->multibulk_reply;
                        	rp->multibulk_reply = NULL;
                        	return RPR_REPLY;
                        }
                        else {
                        	continue;
                        }
                    }
                    else {
                        return RPR_REPLY;
                    }
                }
                break;
            }
            //start multibulk reply
            case 13: {
                if(c == '-') { //nil multibulk reply
                    rp->p++;
                    rp->cs = 14; 
                    continue;
                }
                else if(isdigit(c)) { //normal multibulk reply
                    MARK;
                    rp->p++;
                    rp->cs = 17;
                    continue;
                }
                break;
            }
            //start nil multibulk reply
            case 14: {
                if(c == '1') {
                    rp->p++;
                    rp->cs = 15;
                    continue;
                }
                break;
            }
            case 15: {
                if(c == CR) {
                    rp->p++;
                    rp->cs = 16;
                    continue;
                }
                break;
            }
            case 16: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 0;
                    *reply = Reply_new(RT_MULTIBULK_NIL, NULL, 0, 0);
                    return RPR_REPLY;
                }
                break;
            }
            //start normal multibulk reply
            case 17: {
                if(c == CR) { //end of digits
                    rp->multibulk_count = atoi(Buffer_data(buffer) + rp->mark);
                    rp->multibulk_reply = Reply_new(RT_MULTIBULK, NULL, 0, rp->multibulk_count);
                    rp->p++;
                    rp->cs = 18;
                    continue;
                }
                else if(isdigit(c)) { //one more digit
                    rp->p++;
                    continue;
                }
                break;
            }
            case 18: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 0;
	            if(rp->multibulk_count == 0) {
			//multi bulk reply with 0 entries
                	*reply = rp->multibulk_reply;
                	rp->multibulk_reply = NULL;
                	return RPR_REPLY;
                    }
                    continue;
                }
                break;
            }
			//integer reply
            case 19: {
                if(c == CR) {
                    rp->p++;
                    rp->cs = 20;
                    continue;
                }
                else {
                    rp->p++;
                    continue;
                }
                break;
            }
            case 20: {
                if(c == LF) {
                    rp->p++;
                    rp->cs = 0;
                    //report integer data
                    *reply = Reply_new(RT_INTEGER, buffer, rp->mark, rp->p - rp->mark - 2);
                    return RPR_REPLY;
                }
                break;
            }
        }
        return RPR_ERROR;
    }
    DEBUG(("exit rp pos: %d len: %d cs: %d\n", rp->p, len, rp->cs));
    assert(rp->p == len);
    return RPR_MORE;
}

