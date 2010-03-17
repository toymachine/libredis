typedef struct _ReplyParser ReplyParser;

typedef enum _ReplyParserResult
{
    RPR_DONE = 0,
    RPR_ERROR = 1,
    RPR_MORE = 2,

    RPR_OK_LINE = 3,
    RPR_ERROR_LINE = 4,
    RPR_BULK_NIL = 5,
    RPR_BULK_VALUE = 6,
    RPR_MULTIBULK_NIL = 7,
    RPR_MULTIBULK_COUNT = 8,
} ReplyParserResult;

size_t ReplyParser_length(ReplyParser *rp);
size_t ReplyParser_offset(ReplyParser *rp);
int ReplyParser_multibulk_count(ReplyParser *rp);
int ReplyParser_init(ReplyParser *rp);
ReplyParser *ReplyParser_new();
ReplyParserResult ReplyParser_execute(ReplyParser *rp, Byte *buffer, size_t len);

