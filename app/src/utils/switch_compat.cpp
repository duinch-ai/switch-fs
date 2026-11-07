#if defined(__SWITCH__)
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>

// Increase main thread stack size from default 128KB to 4MB for Borealis C++ GUI
extern "C" u32 __nx_applet_stack_size = 0x400000;

struct passwd;
struct group;

extern "C" {

mode_t umask(mode_t mask) {
    (void)mask;
    return 0022;
}

struct passwd *getpwnam(const char *name) {
    (void)name;
    return nullptr;
}

struct group *getgrnam(const char *name) {
    (void)name;
    return nullptr;
}

struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    return nullptr;
}

struct group *getgrgid(gid_t gid) {
    (void)gid;
    return nullptr;
}

} // extern "C"
#endif
