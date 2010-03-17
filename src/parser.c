#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "parser.h"

#define MARK rp->mark = rp->p

struct _ReplyParser
{
    size_t p; //position
    int cs; //state
    int bulk_count; //number of chars to read for current binary safe bulk-value
    int multibulk_count; //the number of bulk replies to read for the current multibulk reply
    size_t mark; //helper to mark start of interesting data

    size_t offset;
    size_t len;

};

size_t ReplyParser_length(ReplyParser *rp)
{
	return rp->len;
}

size_t ReplyParser_offset(ReplyParser *rp)
{
	return rp->offset;
}

int ReplyParser_multibulk_count(ReplyParser *rp)
{
	return rp->multibulk_count;
}

int ReplyParser_init(ReplyParser *rp)
{  
    rp->p = 0;
    rp->cs = 0;  
    rp->bulk_count = 0;
    rp->multibulk_count = 0;
    rp->mark = 0;
    rp->offset = 0;
    rp->len = 0;

    return 0;   
}

ReplyParser *ReplyParser_new()
{
	ReplyParser *rp = REDIS_ALLOC_T(ReplyParser);
	ReplyParser_init(rp);
	return rp;
}


ReplyParserResult ReplyParser_execute(ReplyParser *rp, Byte *buffer, size_t len)
{    
    while((rp->p) < len) {
        Byte c = buffer[rp->p];
        //printf("cs: %d, char: %d\n", rp->cs, c);
        switch(rp->cs) {
            case 0: {
                if(c == '$') {
                    rp->p++;
                    rp->cs = 5;
                    continue;
                }
                else if(rp->multibulk_count == 0) { 
                    //if we are still reading a multibulk reply, we will never go here 
                    //(e.g. we only allow bulk replies within a multibulk reply)                    
                    if(c == '+') {
                        rp->p++;
                        rp->cs = 1;
                        MARK;
                        continue;
                    }
                    else if(c == '-') {
                        rp->p++;
                        rp->cs = 3;
                        MARK;
                        continue;
                    }
                    else if(c == '*') {
                        rp->p++;
                        rp->cs = 13;
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
                    rp->offset = rp->mark;  
                    rp->len = rp->p - rp->mark - 2;
                    return RPR_OK_LINE;
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
                    rp->offset = rp->mark;  
                    rp->len = rp->p - rp->mark - 2;
                    return RPR_ERROR_LINE;
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
                    if(rp->multibulk_count > 0) {
                        rp->multibulk_count -= 1;
                    }
                    return RPR_BULK_NIL;
                }
                break;
            }
            //end nil bulk reply
            //start normal bulk reply
            case 9: {
                if(c == CR) { //end of digits
                    rp->bulk_count = atoi(buffer + rp->mark);
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
                    if(rp->multibulk_count > 0) {
                        rp->multibulk_count -= 1;
                    }
                    rp->offset = rp->mark;
                    rp->len = rp->p - rp->mark - 2;
                    return RPR_BULK_VALUE;
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
                    return RPR_MULTIBULK_NIL; 
                }
                break;
            }
            //start normal multibulk reply
            case 17: {
                if(c == CR) { //end of digits
                    rp->multibulk_count = atoi(buffer + rp->mark);
                    rp->p++;
                    rp->cs = 18;
                    return RPR_MULTIBULK_COUNT;
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
                    continue;
                }
                break;
            }

        }
        return RPR_ERROR;
    }
    assert(rp->p == len);
    //printf("exec exit pos: %d len: %d cs: %d\n", rp->p, len, rp->cs);
    return rp->cs == 0 ? RPR_DONE : RPR_MORE;
}

