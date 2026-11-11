#ifndef YT2_STATUS_H
#define YT2_STATUS_H

/*
 * B2 result codes.  The real C port has no guest status word or carry flag:
 * functions report success/failure explicitly and callers decide what to do.
 */
typedef enum yt2_status {
    YT2_STATUS_OK = 0,
    YT2_STATUS_ERROR = 1
} yt2_status;

#endif /* YT2_STATUS_H */
