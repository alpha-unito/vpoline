#!/bin/bash

VPOLINE_TEST=$HOME/vpoline/test
SYSCALL_HOOK=$VPOLINE_TEST/intercept_sys_forward
VPOLINE_HOOK=$VPOLINE_TEST/hook_forward

gcc -o ${SYSCALL_HOOK}.so ${SYSCALL_HOOK}.c -fpic -shared -I"$HOME/syscall_intercept/include" -L"$HOME/syscall_intercept/build" -lsyscall_intercept
gcc -o ${VPOLINE_HOOK}.so ${VPOLINE_HOOK}.c -fpic -shared -I"$HOME/vpoline/include"