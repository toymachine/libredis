#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <event.h>

#include "redis.h"
#include "batch.h"
#include "parser.h"
#include "reply.h"

int main2(void)
{
	printf("bla\n");

	//Alloc alloc = { Alloc_alloc, Alloc_free };

    ReplyParser *rp = ReplyParser_new();


    //char *buffer = "+blaat aap\r\n";
    //char *buffer = "-blaat aap\r\n";
    //char *buffer = "$-1\r\n";
    //char *buffer = "$7\r\naappiet\r\n";
    //char *buffer = "*-1\r\n";
    char *buffer = "*3\r\n$4\r\nbar1\r\n$4\r\nbar2\r\n$4\r\nbar3\r\n";
    //char *buffer = "*3\r\n$4\r\nbar1\r\n$4\r\nbar2\r\n-blaatbar3\r\n";
    //Byte *buffer = (Byte *)"*3\r\n$4\r\nbar1\r\n$4\r\nbar2\r\n$-1\r\n$4\r\nbar4\r\n+blaatbar\r\n-errorbar\r\n*-1\r\n";

    //printf("buffer '%s'\n", buffer);

    //res = ReplyParser_execute(&rp, buffer, 0);
    //printf("res: %d, cs: %d\n", res, rp->cs);
    //res = ReplyParser_execute(&rp, buffer, strlen(buffer) - 30);
    //printf("res: %d, cs: %d\n", res, rp->cs);
    int n = strlen(buffer);
    while(1) {
        ReplyParserResult res = ReplyParser_execute(rp, buffer, n);
        switch(res) {
        case RPR_ERROR: {
            printf("error!\n");
            return 0;
        }
        case RPR_DONE: {
            printf("done.\n");
            return 0;
        }
        case RPR_OK_LINE: {
            printf("ok reply, '%.*s'\n", ReplyParser_length(rp), buffer + ReplyParser_offset(rp));
            break;
        }
        case RPR_ERROR_LINE: {
            printf("error reply, '%.*s'\n", ReplyParser_length(rp), buffer + ReplyParser_offset(rp));
            break;
        }
        case RPR_BULK_NIL: {
            printf("nil bulk reply\n");
            break;
        }
        case RPR_BULK_VALUE: {
            printf("bulk reply, '%.*s'\n", ReplyParser_length(rp), buffer + ReplyParser_offset(rp));
            break;
        }
        case RPR_MULTIBULK_COUNT: {
            printf("multibulk reply , items: %d\n", ReplyParser_multibulk_count(rp));
            break;
        }
        case RPR_MULTIBULK_NIL: {
            printf("multibulk nil\n");
            break;
        }
        case RPR_MORE: {
            printf("more...");
            return 0;
        }
        default: {
            printf("unknown exit\n");
            return 0;
        }
        }
        //printf("res: %d, cs: %d\n", res, rp->cs);
    }
    return 0;
}

int main(void) {
	event_init();

	Connection *connection = Connection_new("127.0.0.1", 6379);

	Batch *batch = Batch_new();
	Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "blaat", 3, "aap");
	Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "piet", 7, "jaapaap");

	Batch_execute(batch, connection);

	event_dispatch();

	while(Batch_has_result(batch)) {
		Reply *reply = Batch_next_result(batch);
		switch(reply->type) {
		default:
			printf("unknown reply %d\n", reply->type);
		}
	}
	printf("normal main done!");

	return 0;
}
