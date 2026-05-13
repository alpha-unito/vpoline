#ifndef RISCVPOLINE_WRAPPER_TEST_H
#define RISCVPOLINE_WRAPPER_TEST_H

#define MAX_TESTS 500 // should be enough

struct syscall_test_data {
    /*
     * each test refer to an expected SYS_nr which must be detected as first
     * argument by the hook_function for the test to succeed
     */
    int expected_sys_nr;

    /* if hook_function syscall_number matched expected_sys_nr */
    int matched;

    /* mnemonic glibc wrapper call */
    const char* name;
};

extern struct syscall_test_data tracker[MAX_TESTS];
extern int current_test_idx;
extern int inside_test;

#endif //RISCVPOLINE_WRAPPER_TEST_H