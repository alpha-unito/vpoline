/**
 * Copyright 2026 University of Turin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "wrapper_test.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include <sys/quota.h>
#include <dirent.h>
#include <sched.h>
#include <signal.h>
#include <bits/sched.h>
#include <sys/sendfile.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/personality.h>
#include <sys/time.h>
#include <linux/module.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/reboot.h>
#include <sys/fsuid.h>
#include <sys/times.h>
#include <grp.h>
#include <sys/utsname.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/swap.h>
#include <sys/fanotify.h>
#include <sys/uio.h>
#include <features.h>

struct syscall_test_data tracker[MAX_TESTS] = {0};
int current_test_idx = 0;
int inside_test = 0;

#define TEST_LIBC_WRAPPER(sys_nr, func_call) do { \
        if (current_test_idx < MAX_TESTS) { \
            tracker[current_test_idx].name = #func_call; \
            tracker[current_test_idx].expected_sys_nr = sys_nr; \
            inside_test = 1; \
            func_call; \
            inside_test = 0; \
            current_test_idx++; \
        } else { \
            write(1, "Increase MAX_TESTS value! Not enough space\n", 43); \
            exit(EXIT_FAILURE); \
        }\
    } while(0)

void print_report() {
    char report[32768];
    int offset = 0;
    int intercepted = 0;

    offset += snprintf(report + offset, sizeof(report) - offset, "\n--- Interception Coverage ---\n");

    for (int i = 0; i < current_test_idx; i++) {
            if (tracker[i].matched) {
                intercepted++;
                offset += snprintf(report + offset, sizeof(report) - offset,
                                   "[ OK ] %s (NR: %d)\n",
                                   tracker[i].name, tracker[i].expected_sys_nr);
            } else {
                offset += snprintf(report + offset, sizeof(report) - offset,
                                   "[FAIL] %s (NR: %d) NOT intercepted\n",
                                   tracker[i].name, tracker[i].expected_sys_nr);
            }
        if (offset >= sizeof(report) - 100) {
            break;
        }
    }


    float percentage = current_test_idx > 0 ? ((float)intercepted / current_test_idx) * 100.0 : 0.0;

    offset += snprintf(report + offset, sizeof(report) - offset,
                       "---------------------------------\n");
    offset += snprintf(report + offset, sizeof(report) - offset,
                       "Stats: %d/%d intercepted (%.1f%%)\n", intercepted, current_test_idx, percentage);

    /*
     * only write to stdout of this executable, hook has been designed to
     * detect it so that it normally get forwarded to the kernel
     */
    write(1, report, offset);
}

static char buffer[2][0x200];

static const char input[2][sizeof(buffer[0])] = {
    "input_data\x01\x02\x03\n\r\t",
    "other_input_data\x01\x02\x03\n\r\t"};

int fd2[2] = {123, 234};

void *p0 = (void *)0x123000;
void *p1 = (void *)0x234000;
void *p2 = (void *)0x456000;

struct stat statbuf;
struct stat64 statbuf64;
struct iovec iovecbuf[4];
struct utsname uname_buf;
struct timespec ts;
unsigned cpu, node;
struct sysinfo s_info;
char *mq_name = "/test_mq";
socklen_t sl[2] = {1, 1};

int main() {

    /*
     * clock_gettime, clock_getres, getcpu, settimeofday are not tested here
     * as prefer calling a function from memory instead of executing ecall
     * explicitly; for this reason it would be pointless to the glibc wrapper
     * coverage
     */

    size_t len0 = strlen(input[0]);

    TEST_LIBC_WRAPPER(SYS_setxattr,setxattr(input[0], input[1], input[1], 3, XATTR_CREATE));
    TEST_LIBC_WRAPPER(SYS_lsetxattr,lsetxattr(input[0], input[1], input[1], 3, XATTR_CREATE));
    TEST_LIBC_WRAPPER(SYS_fsetxattr,fsetxattr(4, input[1], input[1], 3, XATTR_REPLACE));
    TEST_LIBC_WRAPPER(SYS_getxattr,getxattr(input[0], input[1], p0, 3));
    TEST_LIBC_WRAPPER(SYS_lgetxattr,lgetxattr(input[0], input[1], p0, 3));
    TEST_LIBC_WRAPPER(SYS_fgetxattr,fgetxattr(4, input[1], p0, 3));
    TEST_LIBC_WRAPPER(SYS_listxattr,listxattr(input[0], p0, 4));
    TEST_LIBC_WRAPPER(SYS_llistxattr,llistxattr(input[0], p0, 4));
    TEST_LIBC_WRAPPER(SYS_flistxattr,flistxattr(5, p0, 4));
    TEST_LIBC_WRAPPER(SYS_removexattr,removexattr(input[0], input[1]));
    TEST_LIBC_WRAPPER(SYS_lremovexattr,lremovexattr(input[0], input[1]));
    TEST_LIBC_WRAPPER(SYS_fremovexattr,fremovexattr(7, input[1]));

    TEST_LIBC_WRAPPER(SYS_getcwd,getcwd(buffer[0],9));

    TEST_LIBC_WRAPPER(SYS_eventfd2,eventfd(47, EFD_SEMAPHORE));

    TEST_LIBC_WRAPPER(SYS_epoll_create1,epoll_create1(0));
    TEST_LIBC_WRAPPER(SYS_epoll_ctl,epoll_ctl(2L, 3L, 4L, p0));

    TEST_LIBC_WRAPPER(SYS_dup,dup(4));
    /* dup2 glibc wrapper actually sends SYS_dup3 to kernel */
    TEST_LIBC_WRAPPER(SYS_dup3,dup2(4,5));
    TEST_LIBC_WRAPPER(SYS_dup3,dup3(4,5, O_CLOEXEC));
    TEST_LIBC_WRAPPER(SYS_fcntl,fcntl(1, F_DUPFD_CLOEXEC, 3, 4));

    TEST_LIBC_WRAPPER(SYS_inotify_init1,inotify_init());
    TEST_LIBC_WRAPPER(SYS_inotify_init1,inotify_init1(IN_NONBLOCK));
    TEST_LIBC_WRAPPER(SYS_inotify_add_watch,inotify_add_watch(7, input[0], 123));
    TEST_LIBC_WRAPPER(SYS_inotify_rm_watch,inotify_rm_watch(7, 8));

    TEST_LIBC_WRAPPER(SYS_ioctl,ioctl(1, 77, p1));

    TEST_LIBC_WRAPPER(SYS_flock,flock(1, LOCK_EX));
    TEST_LIBC_WRAPPER(SYS_mknodat,mknodat(1, input[0], 1, 2));
    TEST_LIBC_WRAPPER(SYS_mkdirat,mkdir(input[0], 0644));
    TEST_LIBC_WRAPPER(SYS_mkdirat,mkdirat(33, input[0], 0644));
    TEST_LIBC_WRAPPER(SYS_unlinkat,unlink(input[0]));
    TEST_LIBC_WRAPPER(SYS_unlinkat,unlinkat(AT_FDCWD, input[0], AT_REMOVEDIR));
    TEST_LIBC_WRAPPER(SYS_symlinkat,symlink(input[0], input[1]));
    TEST_LIBC_WRAPPER(SYS_symlinkat,symlinkat(input[0], 7, input[1]));
    TEST_LIBC_WRAPPER(SYS_linkat,link(input[0], input[1]));
    TEST_LIBC_WRAPPER(SYS_linkat,linkat(1, input[0], 2, input[1], 0));

    TEST_LIBC_WRAPPER(SYS_umount2,umount2(input[0], MNT_DETACH));
    TEST_LIBC_WRAPPER(SYS_mount,mount(input[0], input[1], p0, 0, p1));

    TEST_LIBC_WRAPPER(SYS_pivot_root,pivot_root(input[0], buffer[0]));

    TEST_LIBC_WRAPPER(SYS_statfs,statfs(input[0], p0));
    TEST_LIBC_WRAPPER(SYS_fstatfs,fstatfs(4, p0));
    TEST_LIBC_WRAPPER(SYS_truncate,truncate(input[0], 4));
    TEST_LIBC_WRAPPER(SYS_ftruncate,ftruncate(3, 3));

    TEST_LIBC_WRAPPER(SYS_fallocate,posix_fallocate(1, 3, 4));
    TEST_LIBC_WRAPPER(SYS_fallocate,posix_fallocate64(1, 3, 4));
    TEST_LIBC_WRAPPER(SYS_fallocate,fallocate64(1, FALLOC_FL_PUNCH_HOLE, 3, 4));
    TEST_LIBC_WRAPPER(SYS_faccessat,faccessat(AT_FDCWD, input[0], X_OK, 0));
    TEST_LIBC_WRAPPER(SYS_chdir,chdir(input[0]));
    TEST_LIBC_WRAPPER(SYS_fchdir,fchdir(6));
    TEST_LIBC_WRAPPER(SYS_chroot,chroot(input[0]));
    TEST_LIBC_WRAPPER(SYS_fchmod,fchmod(4, 0644));
    TEST_LIBC_WRAPPER(SYS_fchmodat,fchmodat(AT_FDCWD, input[0], 0644, 0));
    TEST_LIBC_WRAPPER(SYS_fchownat,fchownat(AT_FDCWD, input[0], 2, 3, 0));
    TEST_LIBC_WRAPPER(SYS_fchown,fchown(4, 2, 3));
    TEST_LIBC_WRAPPER(SYS_openat,openat(AT_FDCWD, input[0], O_RDONLY, 0321));
    TEST_LIBC_WRAPPER(SYS_close,close(9));

    TEST_LIBC_WRAPPER(SYS_pipe2,pipe(fd2));
    TEST_LIBC_WRAPPER(SYS_pipe2,pipe2(fd2, 0));

    TEST_LIBC_WRAPPER(SYS_quotactl,quotactl(1, p0, 2, p1));


    DIR *d = opendir(".");
    if (d) {
        TEST_LIBC_WRAPPER(SYS_getdents64, readdir(d));
        closedir(d);
    }
    TEST_LIBC_WRAPPER(SYS_lseek,lseek(0, 0, SEEK_SET));
    TEST_LIBC_WRAPPER(SYS_read,read(7, buffer[0], len0 + 3));
    TEST_LIBC_WRAPPER(SYS_write,write(7, input, 11));
    TEST_LIBC_WRAPPER(SYS_pread64,pread64(7, buffer[0], len0 + 3, ((size_t)UINT32_MAX) + 16));

    TEST_LIBC_WRAPPER(SYS_sendfile,sendfile(6, 7, p0, 99));

    TEST_LIBC_WRAPPER(SYS_signalfd4,signalfd(1, p0, 13));

    TEST_LIBC_WRAPPER(SYS_readlinkat,readlinkat(2, input[0], buffer[0], len0));
    TEST_LIBC_WRAPPER(SYS_newfstatat,fstatat(AT_FDCWD, input[0], &statbuf, 0));
    TEST_LIBC_WRAPPER(SYS_fstat,fstat64(2, &statbuf64));

    TEST_LIBC_WRAPPER(SYS_sync,sync());
    TEST_LIBC_WRAPPER(SYS_timerfd_create,timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC));
    TEST_LIBC_WRAPPER(SYS_timerfd_settime,timerfd_settime(1, TFD_TIMER_ABSTIME, p0, p1));
    TEST_LIBC_WRAPPER(SYS_timerfd_gettime,timerfd_gettime(2, p0));

    TEST_LIBC_WRAPPER(SYS_utimensat,utimensat(AT_FDCWD, input[0], p0, 0));

    TEST_LIBC_WRAPPER(SYS_acct,acct(input[0]));

    TEST_LIBC_WRAPPER(SYS_capget,capget(NULL, NULL));
    TEST_LIBC_WRAPPER(SYS_capset,capset(NULL, NULL));

    TEST_LIBC_WRAPPER(SYS_personality,personality(0));

    TEST_LIBC_WRAPPER(SYS_unshare,unshare(CLONE_FILES | CLONE_NEWNS));

    TEST_LIBC_WRAPPER(SYS_getitimer,getitimer(3, p0));
    TEST_LIBC_WRAPPER(SYS_setitimer,setitimer(6, p0, p1));

    TEST_LIBC_WRAPPER(SYS_init_module,init_module(p0, 3, input[0]));
    TEST_LIBC_WRAPPER(SYS_delete_module,delete_module(input[0], O_NONBLOCK | O_TRUNC));

    TEST_LIBC_WRAPPER(SYS_timer_create,timer_create(3, NULL, NULL));
    TEST_LIBC_WRAPPER(SYS_timer_gettime,timer_gettime((timer_t)4, p0));
    TEST_LIBC_WRAPPER(SYS_timer_getoverrun,timer_getoverrun((timer_t)6));
    TEST_LIBC_WRAPPER(SYS_timer_settime,timer_settime((timer_t)4, TFD_TIMER_ABSTIME, p0, p1));
    TEST_LIBC_WRAPPER(SYS_timer_delete,timer_delete((timer_t)4));
    TEST_LIBC_WRAPPER(SYS_clock_settime,clock_settime(CLOCK_REALTIME, &ts));

    TEST_LIBC_WRAPPER(SYS_ptrace,ptrace(0, 1, p0, p1));

    TEST_LIBC_WRAPPER(SYS_sched_setparam,sched_setparam(1, p0));
    TEST_LIBC_WRAPPER(SYS_sched_setscheduler,sched_setscheduler(1, SCHED_BATCH, p0));
    TEST_LIBC_WRAPPER(SYS_sched_getscheduler,sched_getscheduler(1));
    TEST_LIBC_WRAPPER(SYS_sched_getparam,sched_getparam(1, p0));
    TEST_LIBC_WRAPPER(SYS_sched_setaffinity,sched_setaffinity(77, 4, p0));
    TEST_LIBC_WRAPPER(SYS_sched_getaffinity,sched_getaffinity(77, 4, p0));
    TEST_LIBC_WRAPPER(SYS_sched_yield,sched_yield());
    TEST_LIBC_WRAPPER(SYS_sched_get_priority_max,sched_get_priority_max(SCHED_RR));
    TEST_LIBC_WRAPPER(SYS_sched_get_priority_min,sched_get_priority_min(SCHED_RR));
    TEST_LIBC_WRAPPER(SYS_sched_rr_get_interval,sched_rr_get_interval(1, p0));

    TEST_LIBC_WRAPPER(SYS_kill,kill(44, 0));
    TEST_LIBC_WRAPPER(SYS_tgkill,tgkill(44, 55, 0));
    TEST_LIBC_WRAPPER(SYS_sigaltstack,sigaltstack(p0, p1));
    TEST_LIBC_WRAPPER(SYS_rt_sigpending,sigpending(p0));
    TEST_LIBC_WRAPPER(SYS_rt_sigqueueinfo,sigqueue(77, SIGILL, (union sigval)p0));
    TEST_LIBC_WRAPPER(SYS_setpriority,setpriority(1, 2, 3));
    TEST_LIBC_WRAPPER(SYS_getpriority,getpriority(1, 2));
    TEST_LIBC_WRAPPER(SYS_reboot,reboot(0));
    TEST_LIBC_WRAPPER(SYS_setregid,setregid(6, 7));
    TEST_LIBC_WRAPPER(SYS_setgid,setgid(123));
    TEST_LIBC_WRAPPER(SYS_setreuid,setreuid(3, 4));
    TEST_LIBC_WRAPPER(SYS_setuid,setuid(123));
    TEST_LIBC_WRAPPER(SYS_setresuid,setresuid(1, 2, 3));
    TEST_LIBC_WRAPPER(SYS_getresuid,getresuid(p0,p1,p1));
    TEST_LIBC_WRAPPER(SYS_setresgid,setresgid(1, 2, 3));
    TEST_LIBC_WRAPPER(SYS_getresgid,getresgid(p0,p1,p1));
    TEST_LIBC_WRAPPER(SYS_setfsuid,setfsuid(88));
    TEST_LIBC_WRAPPER(SYS_setfsgid,setfsgid(77));
    TEST_LIBC_WRAPPER(SYS_times,times(p0));
    TEST_LIBC_WRAPPER(SYS_setpgid,setpgid(1, 2));
    TEST_LIBC_WRAPPER(SYS_getpgid,getpgid(99));
    TEST_LIBC_WRAPPER(SYS_getsid,getsid(66));
    TEST_LIBC_WRAPPER(SYS_setsid,setsid());
    TEST_LIBC_WRAPPER(SYS_getgroups,getgroups(9, p0));
    TEST_LIBC_WRAPPER(SYS_setgroups,setgroups(9, p0));
    TEST_LIBC_WRAPPER(SYS_uname,uname(&uname_buf));
    TEST_LIBC_WRAPPER(SYS_sethostname,sethostname(input[0], len0));
    TEST_LIBC_WRAPPER(SYS_setdomainname,setdomainname(input[0], len0));

    TEST_LIBC_WRAPPER(SYS_getrusage,getrusage(RUSAGE_SELF, p0));
    TEST_LIBC_WRAPPER(SYS_umask,umask(0222));
    TEST_LIBC_WRAPPER(SYS_prctl,prctl(PR_CAPBSET_DROP, 1, 2, 3, 4););

    TEST_LIBC_WRAPPER(SYS_getpid, getpid());
    TEST_LIBC_WRAPPER(SYS_getppid,getppid());
    TEST_LIBC_WRAPPER(SYS_getuid,getuid());
    TEST_LIBC_WRAPPER(SYS_geteuid,geteuid());
    TEST_LIBC_WRAPPER(SYS_getgid,getgid());
    TEST_LIBC_WRAPPER(SYS_getegid,getegid());
    TEST_LIBC_WRAPPER(SYS_gettid,gettid());
    TEST_LIBC_WRAPPER(SYS_sysinfo,sysinfo(&s_info));

    TEST_LIBC_WRAPPER(SYS_mq_open,mq_open(mq_name, O_CREAT | O_RDWR, 0777, p0));
    TEST_LIBC_WRAPPER(SYS_mq_unlink,mq_unlink(mq_name));
    TEST_LIBC_WRAPPER(SYS_mq_notify,mq_notify(1, NULL));
    TEST_LIBC_WRAPPER(SYS_mq_getsetattr,mq_setattr(1, p0, p1));

    TEST_LIBC_WRAPPER(SYS_msgget,msgget(1, IPC_CREAT));
    TEST_LIBC_WRAPPER(SYS_msgctl,msgctl(1, IPC_STAT, p0));

    TEST_LIBC_WRAPPER(SYS_semget,semget(4, 1, IPC_CREAT));
    TEST_LIBC_WRAPPER(SYS_semctl,semctl(1,2,3));
    TEST_LIBC_WRAPPER(SYS_semtimedop,semtimedop(4, p0, 1, p1));

    TEST_LIBC_WRAPPER(SYS_shmget,shmget(1, 2, IPC_CREAT));
    TEST_LIBC_WRAPPER(SYS_shmctl,shmctl(3, SHM_INFO, p0));
    TEST_LIBC_WRAPPER(SYS_shmat,shmat(3, p0, 5));
    TEST_LIBC_WRAPPER(SYS_shmdt,shmdt(p0));

    TEST_LIBC_WRAPPER(SYS_socket,socket(AF_INET, SOCK_NONBLOCK, 0));
    TEST_LIBC_WRAPPER(SYS_socketpair,socketpair(4, 5, 6, p0));
    TEST_LIBC_WRAPPER(SYS_bind,bind(6, p0, 9));
    TEST_LIBC_WRAPPER(SYS_listen,listen(5, 3));
    TEST_LIBC_WRAPPER(SYS_getsockname,getsockname(4, p0, p1));
    TEST_LIBC_WRAPPER(SYS_getpeername,getpeername(4, p0, p1));
    TEST_LIBC_WRAPPER(SYS_setsockopt,setsockopt(4, 5, 6, p0, 7));
    TEST_LIBC_WRAPPER(SYS_getsockopt,getsockopt(4, 5, 6, p0, sl));
    TEST_LIBC_WRAPPER(SYS_shutdown,shutdown(3, SHUT_RD));

    TEST_LIBC_WRAPPER(SYS_readahead,readahead(4, 0x4321, 123));
    TEST_LIBC_WRAPPER(SYS_brk,brk(p0));
    TEST_LIBC_WRAPPER(SYS_munmap,munmap(p0, 0x1000));
    TEST_LIBC_WRAPPER(SYS_mremap,mremap(p0, ((size_t)UINT32_MAX) + 7,
        ((size_t)UINT32_MAX) + 77, MREMAP_MAYMOVE););
    TEST_LIBC_WRAPPER(SYS_mmap,mmap(p0, 0x8000, PROT_EXEC, MAP_SHARED, 99, 0x1000););
    TEST_LIBC_WRAPPER(SYS_fadvise64,posix_fadvise(3L, 100L, 99L, POSIX_FADV_SEQUENTIAL));

    TEST_LIBC_WRAPPER(SYS_swapon,swapon(input[0], SWAP_FLAG_PREFER));
    TEST_LIBC_WRAPPER(SYS_swapoff,swapoff(input[0]));
    TEST_LIBC_WRAPPER(SYS_mprotect,mprotect(p0, 0x4000, PROT_READ));
    TEST_LIBC_WRAPPER(SYS_mlock,mlock(p0, 0x1000));
    TEST_LIBC_WRAPPER(SYS_munlock,munlock(p0, 0x1000));
    TEST_LIBC_WRAPPER(SYS_mlockall,mlockall(MCL_CURRENT));
    TEST_LIBC_WRAPPER(SYS_munlockall,munlockall());
    TEST_LIBC_WRAPPER(SYS_mincore,mincore(p0, 99, p1));
    TEST_LIBC_WRAPPER(SYS_madvise,madvise(p0, 0x2000, MADV_NORMAL));
    TEST_LIBC_WRAPPER(SYS_remap_file_pages,remap_file_pages(p0, 0x2000, PROT_READ, 0, 0));

    TEST_LIBC_WRAPPER(SYS_prlimit64,getrlimit(RLIMIT_CORE, p0));
    TEST_LIBC_WRAPPER(SYS_prlimit64,setrlimit(RLIMIT_CPU, p0));
    TEST_LIBC_WRAPPER(SYS_prlimit64,prlimit(8, RLIMIT_CORE, p0, p1));
    TEST_LIBC_WRAPPER(SYS_fanotify_init,fanotify_init(FAN_CLASS_PRE_CONTENT | FAN_CLOEXEC, O_RDWR));
    TEST_LIBC_WRAPPER(SYS_fanotify_mark,fanotify_mark(2, FAN_MARK_REMOVE, FAN_Q_OVERFLOW, 3, input[0]));
    TEST_LIBC_WRAPPER(SYS_name_to_handle_at,name_to_handle_at(AT_FDCWD, input[0], p0, p1, 0L));
    TEST_LIBC_WRAPPER(SYS_clock_adjtime,clock_adjtime(CLOCK_REALTIME, p0));
    TEST_LIBC_WRAPPER(SYS_syncfs,syncfs(3));
    TEST_LIBC_WRAPPER(SYS_setns,setns(2, 0));
    TEST_LIBC_WRAPPER(SYS_process_vm_readv,process_vm_readv(1L, p0, 3L, p1, 5L, 6L));
    TEST_LIBC_WRAPPER(SYS_process_vm_writev,process_vm_writev(1L, p0, 3L, p1, 5L, 6L));

#if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
    #if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 41)
        TEST_LIBC_WRAPPER(SYS_sched_setattr,sched_setattr(1, p0, 0));
        TEST_LIBC_WRAPPER(SYS_sched_getattr,sched_getattr(1, p0, 0, 0));
    #endif
#endif

    TEST_LIBC_WRAPPER(SYS_renameat2,rename(input[0], input[1]));
    TEST_LIBC_WRAPPER(SYS_renameat2,renameat(AT_FDCWD, input[0], 7, input[1]));
    TEST_LIBC_WRAPPER(SYS_renameat2,renameat2(AT_FDCWD, input[0], 7, input[1], RENAME_EXCHANGE));
    TEST_LIBC_WRAPPER(SYS_memfd_create,memfd_create(input[0], MFD_CLOEXEC));
    TEST_LIBC_WRAPPER(SYS_mlock2,mlock2(p0, 0x3000, 0));
    TEST_LIBC_WRAPPER(SYS_pkey_mprotect,pkey_mprotect(p0, 0x4000, PROT_READ, 3));
    TEST_LIBC_WRAPPER(SYS_pkey_alloc,pkey_alloc(0, 0));
    TEST_LIBC_WRAPPER(SYS_pkey_free,pkey_free(3));
    TEST_LIBC_WRAPPER(SYS_statx,statx(AT_FDCWD, input[0], AT_STATX_SYNC_AS_STAT, STATX_ALL, p0));
    TEST_LIBC_WRAPPER(SYS_pidfd_send_signal,pidfd_send_signal(4, 5, p0, 0));
    TEST_LIBC_WRAPPER(SYS_open_tree,open_tree(AT_FDCWD, input[0], 0));
    TEST_LIBC_WRAPPER(SYS_move_mount,move_mount(1, input[0], 2, input[1], 0));
    TEST_LIBC_WRAPPER(SYS_fsopen,fsopen(input[0], 0));
    TEST_LIBC_WRAPPER(SYS_fsconfig,fsconfig(3, 4, p0, p1, 0));
    TEST_LIBC_WRAPPER(SYS_fsmount,fsmount(3, 4, 0));
    TEST_LIBC_WRAPPER(SYS_fspick, fspick(3, p0, 0));
    TEST_LIBC_WRAPPER(SYS_pidfd_open, pidfd_open(4, 0));
    TEST_LIBC_WRAPPER(SYS_close_range, close_range(3, 4, 0));
    TEST_LIBC_WRAPPER(SYS_pidfd_getfd, pidfd_getfd(4, 5, 0));
    TEST_LIBC_WRAPPER(SYS_faccessat2,faccessat(AT_FDCWD, input[0], X_OK, AT_SYMLINK_NOFOLLOW));
    TEST_LIBC_WRAPPER(SYS_process_madvise,process_madvise(66, iovecbuf, 0x2000, MADV_NORMAL, 0));
    TEST_LIBC_WRAPPER(SYS_mount_setattr,mount_setattr(2, input[0], 0, p0, 0));
    TEST_LIBC_WRAPPER(SYS_process_mrelease,process_mrelease(66, 0));
#ifdef SYS_fchmodat2
    TEST_LIBC_WRAPPER(SYS_fchmodat2,fchmodat(AT_FDCWD, input[0], 0644, AT_SYMLINK_NOFOLLOW));
#endif

    print_report();

    return 0;
}
