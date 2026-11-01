#include <stddef.h>
#include <darlingserver/rpc.h>
#if defined(__APPLE__)
#include <darling/emulation/conversion/network/recvmsg.h>
#include <mach/message.h>
#include <sys/_types/_iovec_t.h>
#endif
_Static_assert(sizeof(dserver_rpc_callhdr_t) == 16, "RPC call header");
_Static_assert(offsetof(dserver_rpc_callhdr_t, architecture) == 12, "architecture offset");
_Static_assert(sizeof(dserver_rpc_replyhdr_t) == 8, "RPC reply header");
_Static_assert(sizeof(dserver_rpc_call_task_self_trap_t) == 16, "task-self request");
_Static_assert(sizeof(dserver_rpc_reply_task_self_trap_t) == 12, "task-self reply");
_Static_assert(sizeof(dserver_rpc_call_mach_reply_port_t) == 16, "reply-port request");
_Static_assert(sizeof(dserver_rpc_reply_mach_reply_port_t) == 12, "reply-port reply");
_Static_assert(sizeof(dserver_rpc_call_mach_msg_overwrite_t) == 56, "Mach-msg RPC request");
_Static_assert(offsetof(dserver_call_mach_msg_overwrite_t, rcv_msg) == 32, "receive pointer alignment");
_Static_assert(sizeof(dserver_rpc_reply_mach_msg_overwrite_t) == 8, "Mach-msg RPC reply");
#if defined(__APPLE__)
_Static_assert(sizeof(mach_msg_header_t) == 24, "Mach message header");
#if defined(__i386__)
_Static_assert(sizeof(struct linux_msghdr) == 28, "ia32 Linux msghdr");
_Static_assert(sizeof(struct linux_cmsghdr) == 12, "ia32 Linux cmsghdr");
_Static_assert(sizeof(struct iovec) == 8, "ia32 Linux iovec");
#else
_Static_assert(sizeof(struct linux_msghdr) == 56, "Linux64 msghdr");
_Static_assert(sizeof(struct linux_cmsghdr) == 16, "Linux64 cmsghdr");
_Static_assert(sizeof(struct iovec) == 16, "Linux64 iovec");
#endif
#endif
